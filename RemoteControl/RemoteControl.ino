/*
 Name:		RemoteControl.ino
 Created:	11/11/2019 6:11:16 PM
 Author:	YLA
*/

#include "NYG.h"
#include "CalibratedAnalogInput.h"
#include "Multiplexer.h"
#include "Hysteresis.h"
#include "Timer.h"
#include "Toggler.h"
#include "RedGreenLed.h"
#include "..\Interface.h"
#include <ESP8266WiFi.h>

using namespace NYG;

#define MUX_A			D5
#define MUX_B			D6
#define MUX_C			D7
#define ANALOG_INPUT	A0

#define LED_GREEN		D0
#define LED_RED			D2

#define zAxisPin		D4

//---------------------------------------------------------------------------------
static Multiplexer					_multiplexer(MUX_A, MUX_B, MUX_C, ANALOG_INPUT);

static MultiplexerInput				_xAxisPR(_multiplexer, 4),
									_yAxisPR(_multiplexer, 1);

static CalibratedAnalogInput*		_xAxis = CalibratedAnalogInput::Create(_xAxisPR, 50, 5);
static CalibratedAnalogInput*		_yAxis = CalibratedAnalogInput::Create(_yAxisPR, 50, 5);

#define N_AXIS_POSITIONS			102
#define AXIS_POSITION_RATE			(CalibratedAnalogInput::MAX_VAL / N_AXIS_POSITIONS)

static Hysteresis					xAxis(*_xAxis, AXIS_POSITION_RATE),
									yAxis(*_yAxis, AXIS_POSITION_RATE);

static Timer						keep_alive_timer;
static Toggler						toggler;

static RedGreenLed					leds(LED_RED, LED_GREEN);

namespace NYG
{
    struct JoystickState
    {
        int         X, Y;
        bool        button_pressed;
    };
}

static NYG::CarData       data = { 0, 0, false, SIGNAL_NONE };
static NYG::JoystickState prev = { -1, -1, false };

#define USE_BUZZER          1
#define CLOSE_CONNECTION    1

#if !CLOSE_CONNECTION
WiFiClient client;
#endif //CLOSE_CONNECTION

enum
{
    KEEP_ALIVE_MAX_SECONDS = 300,	// 5 minutes
    KEEP_ALIVE_MAX_COUNT   =  30,   // (KEEP_ALIVE_MAX_SECONDS / KEEP_ALIVE_INTERVAL_SECONDS)
};

static int keep_alive_count = 0;

//---------------------------------------------------------------------------------
static bool connect_server(WiFiClient& client)
{
#define MAX_CONNECT_ATTEMPTS    10
    for (int attempts = 0; ; attempts++)
    {
        if (client.connect(HOST_ADDRESS, HOST_PORT))
            break;

        if (attempts < MAX_CONNECT_ATTEMPTS)
        {
            delay(25);
            continue;
        }

        LOGGER << "Connection failed." << NL;
        return false;
    }

	client.setDefaultNoDelay(true);
	client.setNoDelay(true);

	return true;
}
//---------------------------------------------------------------------------------
static bool send()
{
    leds.SetRed();

#if CLOSE_CONNECTION
    WiFiClient client;
    if (!connect_server(client))
        return false;
#endif //CLOSE_CONNECTION

    // Prepare request
    char request[128];
    sprintf(request,
            "GET " 
            URL_CAR_DATA "?"   URL_PARAM_X "=%d&"  URL_PARAM_Y "=%d&"  URL_PARAM_FAST "=%d&"   URL_PARAM_SIGNAL "=%d" //url
            " HTTP/1.1\r\n"
            "Host: " HOST_ADDRESS "\r\n"
#if CLOSE_CONNECTION
            "Connection: close\r\n"
#endif //CLOSE_CONNECTION
            "\r\n",
            data.X, data.Y, (int)data.FAST, (int)data.SIGNAL);

    // Send & close
	client.print(request);

#if CLOSE_CONNECTION
    client.stop();
#endif //CLOSE_CONNECTION

    sprintf(request,
            URL_PARAM_X "=%d, "  URL_PARAM_Y "=%d, "  URL_PARAM_FAST "=%d, "   URL_PARAM_SIGNAL "=%d",
            data.X, data.Y, (int)data.FAST, (int)data.SIGNAL);

    LOGGER << request << NL;

    leds.SetGreen();

    delay(10);

    return true;
}
//---------------------------------------------------------------------------------
static bool send(const NYG::JoystickState& curr, bool button_was_previouselly_pressed)
{
    // Calculate coordinates
    data.X = curr.X - 512;
    data.Y = curr.Y - 512;

    data.X = (data.X / AXIS_POSITION_RATE) * AXIS_POSITION_RATE;
    data.Y = (data.Y / AXIS_POSITION_RATE) * AXIS_POSITION_RATE;

    // Set speed / lights

    if (curr.button_pressed != button_was_previouselly_pressed)
    {
        static uint32_t last_button_event_millis;
#if USE_BUZZER
        static uint32_t last_signal_toggled_millis = 0;
#endif //USE_BUZZER
        const uint32_t now = millis();

        if (curr.button_pressed)
        {
            LOGGER << "Pressed" << NL;
            last_button_event_millis = now;

            //if (!memcmp(&data, &prev, sizeof(data)))
                return true;
        }
        else
        {
            LOGGER << "Released" << NL;
            uint32_t delta = now - last_button_event_millis;

            if (delta <= 2500)  // Short click
            {
#if USE_BUZZER
                delta = (last_signal_toggled_millis) ? now - last_signal_toggled_millis : 0;
                
                if (delta && delta <= 800)
                {
                    // Double click
                    data.SIGNAL = (data.SIGNAL != SIGNAL_SOUND) ? SIGNAL_SOUND : SIGNAL_LIGHTS;
                    last_signal_toggled_millis = 0;
                }
                else
                {
                    // Single click
                    last_signal_toggled_millis = now;
                    data.SIGNAL = (data.SIGNAL != SIGNAL_LIGHTS) ? SIGNAL_LIGHTS : SIGNAL_NONE;
                }
#else
                data.SIGNAL = (data.SIGNAL != SIGNAL_LIGHTS) ? SIGNAL_LIGHTS : SIGNAL_NONE;
                last_button_event_millis = 0;
#endif //USE_BUZZER

            }
            else      // Long press click
            {
                data.FAST = !data.FAST;
            }
        }
    }

    #define StartKeepAliveTimer()	keep_alive_timer.StartForever(1000)
    toggler.Stop();
    StartKeepAliveTimer();

    keep_alive_count = 0;

    return send();
}
//---------------------------------------------------------------------------------
static bool keep_alive()
{
    if (keep_alive_timer.Test())
    {
        if (keep_alive_count >= KEEP_ALIVE_MAX_COUNT)
        {
            LOGGER << "Keep-alive max. count " << keep_alive_count << " reached" << NL;
            leds.SetOff();

            toggler.StartOnOff(leds.GetRed(), 500);

            return true;
        }

        keep_alive_count++;

        return send();
    }

    return true;
}
//---------------------------------------------------------------------------------
void loop()
{
	if (!_xAxis || !_yAxis)
		return;

	if (keep_alive_count >= KEEP_ALIVE_MAX_COUNT)
	{
		toggler.Toggle();
	}

    NYG::JoystickState curr = prev;

	curr.X = xAxis.Get();
	curr.Y = yAxis.Get();
    curr.button_pressed = !digitalRead(zAxisPin);

    if (memcmp(&prev, &curr, sizeof(curr)))
    {
        if (send(curr, prev.button_pressed))
        {
            prev = curr;
        }
        else
        {
            leds.SetRed();
            delay(500);
            leds.SetOff();
            delay(500);
        }
    }
    else
    {
        keep_alive();
    }
}
//---------------------------------------------------------------------------------
void setup()
{
	Serial.begin(115200);

	delay(1000);

	{
		leds.SetRed();

		WiFi.mode(WIFI_STA);
		WiFi.begin(WIFI_SSID, WIFI_PASSCODE);

		LOGGER << "Connecting to WiFi... ";

		while (WiFi.status() != WL_CONNECTED)
		{
			leds.Toggle();
			delay(1000);
		}

		WiFi.setAutoConnect(true);

		LOGGER << "Done!" << NL;
	}

	leds.SetRed();

	{
		LOGGER << "Connecting to Car... ";

#if CLOSE_CONNECTION
        WiFiClient client;
#endif CLOSE_CONNECTION

		while (!client.connect(HOST_ADDRESS, HOST_PORT))
		{
//			LOGGER << "FAILED." << NL;
			leds.Toggle();
			delay(250);
		}

#if CLOSE_CONNECTION
        client.stop();
#endif CLOSE_CONNECTION

		leds.SetGreen();

		LOGGER << "Done!" << NL;
	}

	StartKeepAliveTimer();

	LOGGER << "Ready!" << NL;
}
//---------------------------------------------------------------------------------
