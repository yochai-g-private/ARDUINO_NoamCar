// NodeMCU Based WIFI Controlled Car//

#define TEST_CALCULATION	0


#define ENA     D5			// Enable/speed motors Right        
#define ENB     D6			// Enable/speed motors Left         
#define IN_1    D8			// L298N in1 motors Right           
#define IN_2    D7			// L298N in2 motors Right           
#define IN_3    D4			// L298N in3 motors Left            
#define IN_4    D3			// L298N in4 motors Left            

#define SIRENA  D0
#define RED     D1
#define BLUE    D2

// remaining digital output pins: 0,1,2,9,10
#include "NYG.h"
#include "Timer.h"
#include "Toggler.h"
#include "RedGreenLed.h"
#include "..\Interface.h"
#include <EEPROM.h>
#include "EepromIO.h"

using namespace NYG;

#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>

static ESP8266WebServer server(80);

static Toggler          light_toggeler,
                        connection_lost_toggler;
static ToneOutputPin    sirena(SIRENA);
static RedGreenLed      chakalaka_leds(RED, BLUE);
static RedGreenLed::RG  chakalaka(chakalaka_leds);

static bool data_available  = false;
static bool is_moving       = false;

#define MAX_SETTINGS_TEXT_SIZE  512

//------------------------------------------------------
//	PREDECLARATIONs
//------------------------------------------------------

static void HTTP_handleCarData();
static void HTTP_handleSettingsSirena();
static void HTTP_handleSettingsMotor();
static void HTTP_handleSettingsLostSIgnal();
static void HTTP_handleSettingsView();
static void HTTP_notFound();

static bool process_data();
static void on_connection_lost();
static void next_sirena_sound();

static void send_OK();
static void send_BadRequest();

//------------------------------------------------------
//	SETUP
//------------------------------------------------------
void setup()
{
    NYG::CarConfig temp;

    EepromInput cfg_input;

    cfg_input >> temp;

    if (memcmp(temp.name, cfg.name, sizeof(cfg.name)))
    {
        EepromOutput cfg_output;
        cfg_output << cfg;
    }
    else
    {
        cfg = temp;
    }

	// Connecting WiFi

	WiFi.mode(WIFI_AP);
	WiFi.softAP(WIFI_SSID, WIFI_PASSCODE, 1, WIFI_HIDDEN);
	IPAddress myIP = WiFi.softAPIP();

	LOGGER << "IP address: " << myIP.toString() << NL;

	// Starting WEB-server 
	server.on(URL_CAR_DATA,	            HTTP_handleCarData);
    server.on("/settings/sirena",       HTTP_handleSettingsSirena);
    server.on("/settings/motor",        HTTP_handleSettingsMotor);
    server.on("/settings/lost_signal",  HTTP_handleSettingsLostSIgnal);
    server.on("/settings",              HTTP_handleSettingsView);
    server.onNotFound(HTTP_notFound);
	server.begin();

    //for (;;) { chakalaka_leds.SetRed(); delay(1000); chakalaka_leds.SetGreen(); delay(1000); }

    on_connection_lost();

    LOGGER << "Ready!" << NL;
}

//------------------------------------------------------
//	LOOP
//------------------------------------------------------
void loop()
{
    static Timer keep_alive_timer;

    light_toggeler.Toggle();
    connection_lost_toggler.Toggle();
    next_sirena_sound();

    server.handleClient();

    if (data_available)
    {
        connection_lost_toggler.Stop();

        keep_alive_timer.StartOnce(KEEP_ALIVE_TIMEOUT_MILLIS);
        data_available = false;

        if (process_data())
            send_OK();
        else
            send_BadRequest();
    }
    else
    {
        if (keep_alive_timer.Test())
        {
            on_connection_lost();
        }
    }
}
//------------------------------------------------------
static CarData  curr,
                prev = { -1 };

static bool GetCoordinateFromHtmlArg(const char* arg_name, int& V);
static bool GetBoolFromHtmlArg(const char* arg_name, bool& V);
static bool GetSignalTypeFromHtmlArg(const char* arg_name, SignalType& V);
static void start_chakalaka();
static void stop_chakalaka();
static void start_sirena();
static void stop_sirena();
static void drive();
//------------------------------------------------------
static bool process_data()
{
	if (server.args() != 4)
		return false;
    
    #define GET_HTML_PARAM(func, id)    if (!func(URL_PARAM_##id, curr.id))    { LOGGER << "URL_PARAM_" #id << NL; return false; }
    
    //LOGGER << __LINE__ << NL;
    GET_HTML_PARAM(GetCoordinateFromHtmlArg, X);
    //LOGGER << __LINE__ << NL;
    GET_HTML_PARAM(GetCoordinateFromHtmlArg, Y);
    //LOGGER << __LINE__ << NL;
    GET_HTML_PARAM(GetBoolFromHtmlArg,       FAST);
    //LOGGER << __LINE__ << NL;
    GET_HTML_PARAM(GetSignalTypeFromHtmlArg, SIGNAL);
    //LOGGER << __LINE__ << NL;

    char request[128];
    sprintf(request,
            URL_PARAM_X "=%d, "  URL_PARAM_Y "=%d, "  URL_PARAM_FAST "=%d, "   URL_PARAM_SIGNAL "=%d",
            curr.X, curr.Y, (int)curr.FAST, (int)curr.SIGNAL);

    LOGGER << request << NL;

    #define CHANGED(fld)    (curr.fld != prev.fld)
    
    if (CHANGED(X) || CHANGED(Y) || CHANGED(FAST))
    {
        //LOGGER << __LINE__ << NL;
        drive();
        //LOGGER << __LINE__ << NL;
    }

    if (CHANGED(SIGNAL))
    {
        //LOGGER << __LINE__ << NL;
        switch (curr.SIGNAL)
        {
            case SIGNAL_NONE:
            {
                //LOGGER << __LINE__ << NL;
                stop_chakalaka();
                stop_sirena();
                break;
            }

            case SIGNAL_LIGHTS:
            {
                //LOGGER << __LINE__ << NL;
                start_chakalaka();
                stop_sirena();

                break;
            }

            case SIGNAL_SOUND:
            {
                //LOGGER << __LINE__ << NL;
                start_chakalaka();
                start_sirena();

                break;
            }
        }
    }

    if (CHANGED(FAST) && !CHANGED(SIGNAL))
    {
        light_toggeler.StartOnOff(chakalaka_leds.GetGreen(), 200, 1);
    }

    //LOGGER << __LINE__ << NL;
    prev = curr;

    //LOGGER << __LINE__ << NL;
    return true;
}
//------------------------------------------------------
static void start_chakalaka()
{
    if (light_toggeler.IsStarted())
        return;

    light_toggeler.StartOnOff(chakalaka, 250);
}
//------------------------------------------------------
static void stop_chakalaka()
{
    light_toggeler.Stop();
    chakalaka_leds.SetOff();
    sirena.Quiet();
}
//------------------------------------------------------
static int      sirena_X = 0;
static uint32_t prev_tone;
//------------------------------------------------------
static void next_sound()
{
    if (is_moving)
    {
#define BASE_FREQ	1000
#define SIN_FACTOR	(BASE_FREQ / 4)

        float sinVal = sin((sirena_X % 180)* (PI / 180));	// Calculate the sine of x
        int   toneVal = BASE_FREQ + sinVal * SIN_FACTOR;    // Calculate sound frequency according to the sine of x

        if(cfg.sirena.use)
            sirena.Tone(toneVal);						    // Output sound frequency to buzzerPin
        else if (sirena.Get())
            sirena.Quiet();

        sirena_X += 5;
    }
    else
    {
        if (sirena.Get())
            sirena.Quiet();
    }
}
//------------------------------------------------------
bool sirena_started = false;
static void start_sirena()
{
    if (sirena_started)
        return;

    sirena_started = true;

    sirena_X = 0;

    prev_tone = millis();
    next_sound();
}
//------------------------------------------------------
static void stop_sirena()
{
    sirena_started = false;
    sirena.Quiet();
}
//------------------------------------------------------
static void next_sirena_sound()
{
    if (sirena_started)
    {
        uint32_t now = millis();
        if (cfg.sirena.duration <= now - prev_tone)
        {
            prev_tone = now;
            next_sound();
        }
    }
}
//------------------------------------------------------
static void on_connection_lost()
{
    curr.X = curr.Y = 0;
    curr.FAST       = false;
    curr.SIGNAL     = SIGNAL_NONE;
    
    drive();

    stop_chakalaka();
    stop_sirena();

    prev = curr;

    class Lost : public IDigitalOutput
    {
        void Set(bool on)
        {
            chakalaka_leds.GetRed().Set(on);
            if (on)
                sirena.Tone(cfg.lost_signal.pitch);
            else
                sirena.Quiet();
        }

        bool Get()	const
        {
            return sirena.Get();
        }

    };

    static Lost lost;

    connection_lost_toggler.Start(lost, Toggler::OnTotal(cfg.lost_signal.duration, cfg.lost_signal.frequency_seconds * 1000));
}
//------------------------------------------------------
void CalculateSpeed(double X, double Y, bool fast, int16_t& right_speed, int16_t& left_speed);

//------------------------------------------------------
//	Side class
//------------------------------------------------------
struct Side
{
    Side(uint8_t p0, uint8_t p1, uint8_t en)
    {
        IN_pin[0] = p0;
        IN_pin[1] = p1;
        EN_pin = en;

        pinMode(IN_pin[0], OUTPUT);
        pinMode(IN_pin[1], OUTPUT);
        pinMode(EN_pin,    OUTPUT);

        Update(0);
    }

    void Update(int16_t speed)
    {
        digitalWrite(IN_pin[0], (speed > 0) ? LOW : HIGH);
        digitalWrite(IN_pin[1], (speed < 0) ? LOW : HIGH);
        analogWrite(EN_pin, 2 * ((speed > 0) ? speed : -speed));
    }

private:

    uint8_t IN_pin[2];
    uint8_t EN_pin;

};

Side right(IN_1, IN_2, ENA),
     left (IN_3, IN_4, ENB);
//------------------------------------------------------
static void drive()
{
    int16_t right_speed, left_speed;

    CalculateSpeed(curr.X, curr.Y, curr.FAST, right_speed, left_speed);

    is_moving = right_speed || left_speed;

    //LOGGER << "L=" << left_speed << "; R=" << right_speed;

    if (left_speed > 1023 || right_speed > 1023)
    {
        LOGGER << " <=============== BUG" << NL;
        return;
    }

    //LOGGER << NL;

    right.Update(right_speed);
    left.Update(left_speed);

}
//------------------------------------------------------

//------------------------------------------------------
//	HTTP arguments
//------------------------------------------------------

static bool GetStringParam(const char* arg_name, String& arg)
{
    arg = server.arg(arg_name);
    const char* s = arg.c_str();

    return s && *s;
}
//------------------------------------------------------
static bool GetIntParam(const char* arg_name, long& N)
{
    String arg;
    
    if (GetStringParam(arg_name, arg))
    {
        N = arg.toInt();
        return true;
    }

    return false;
}
//------------------------------------------------------
static bool GetRealParam(const char* arg_name, double& N)
{
    String arg;

    if (GetStringParam(arg_name, arg))
    {
        N = arg.toDouble();
        return true;
    }

    return false;
}
//------------------------------------------------------
static bool GetCoordinateFromHtmlArg(const char* arg_name, int& V)
{
    long param;

    if (GetIntParam(arg_name, param))
    {
        V = (int)param;
        return param == (long)V;
    }

    return false;
}
//------------------------------------------------------
static bool GetBoolFromHtmlArg(const char* arg_name, bool& V)
{
    long param;

    if (GetIntParam(arg_name, param))
    {
        V = (bool)param;
        return param == (long)V;
    }

    return false;
}
//------------------------------------------------------
static bool GetSignalTypeFromHtmlArg(const char* arg_name, SignalType& V)
{
    long param;

    if (GetIntParam(arg_name, param))
    {
        V = (SignalType)param;
        return  param == (long)V     &&
                V >= __SIGNAL_MIN__ &&
                V <= __SIGNAL_MAX__;
    }

    return false;
}
//------------------------------------------------------

//------------------------------------------------------
//	HTTP handles
//------------------------------------------------------

static void HTTP_handleCarData()
{
    data_available = true;
}
//------------------------------------------------------
static void HTTP_notFound()
{
    LOGGER << "HTTP_notFound: " << server.uri() << NL;
    server.send(400, "text/html", "Not found!!!");
    delay(1);
}
//------------------------------------------------------
static bool update_cfg(const NYG::CarConfig& temp)
{
    if (memcmp(&cfg, &temp, sizeof(cfg)))
    {
        cfg = temp;
        EepromOutput cfg_output;
        cfg_output << cfg;
        return true;
    }

    return false;
}
//------------------------------------------------------
static const char* get_sirena_settings(char* text)
{
    sprintf(text,
            "<h1>Sirena:</h1><p>"
            "<p style=\"text-indent: 4em;\">use      = %s</p>"
            "<p style=\"text-indent: 4em;\">duration = %d</p></p>",
            cfg.sirena.use ? "YES" : "NO",
            cfg.sirena.duration);

    return text;
}
//------------------------------------------------------
static void HTTP_handleSettingsSirena()
{
    NYG::CarConfig temp = cfg;

    String use;

    if (GetStringParam("use", use))
    {
        use.toUpperCase();
        if (use == "YES" || use == "1")
            temp.sirena.use = true;
        else if (use == "NO" || use == "0")
            temp.sirena.use = false;
    }

    #define GET_NUMERIC_PARAM(type, fld, min, max, val, parent_fld)    {\
        if (Get##type##Param(#fld,val))    {\
            if (val < min)          val = min;\
            else if (val > max)     val = max;\
            temp.parent_fld.fld = val;    } }

    long N;

    GET_NUMERIC_PARAM(Int, duration, 10, 200, N, sirena);
    
    update_cfg(temp);

    char text[MAX_SETTINGS_TEXT_SIZE];
    server.send(200, "text/html", get_sirena_settings(text));
}
//------------------------------------------------------
static const char* get_motor_settings(char* text)
{
    sprintf(text,
            "<h1>Motor:</h1><p>"
            "<p style=\"text-indent: 4em;\">start_moving_threshold = %d</p>"
            "<p style=\"text-indent: 4em;\">min_speed              = %d</p>"
            "<p style=\"text-indent: 4em;\">max_speed_slow         = %d</p>"
            "<p style=\"text-indent: 4em;\">max_speed_fast         = %d</p>"
            "<p style=\"text-indent: 4em;\">factor                 = %lf</p></p>",
            cfg.motor.start_moving_threshold,
            cfg.motor.min_speed,
            cfg.motor.max_speed_slow,
            cfg.motor.max_speed_fast,
            cfg.motor.factor );

    return text;
}
//------------------------------------------------------
static void HTTP_handleSettingsMotor()
{
    NYG::CarConfig temp = cfg;

    long N;

    #define GET_MOTOR_INT_PARAM(fld, min, max)      GET_NUMERIC_PARAM(Int, fld, min, max, N, motor)

    GET_MOTOR_INT_PARAM(start_moving_threshold,  20,  100);
    GET_MOTOR_INT_PARAM(min_speed,               50,  150);
    GET_MOTOR_INT_PARAM(max_speed_slow,         200,  500);
    GET_MOTOR_INT_PARAM(max_speed_fast,         600, 1000);

    double D;

    GET_NUMERIC_PARAM(Real, factor, 1, 5, D, motor);

    update_cfg(temp);

    char text[MAX_SETTINGS_TEXT_SIZE];
    server.send(200, "text/html", get_motor_settings(text));
}
//------------------------------------------------------
static const char* get_lost_signal_settings(char* text)
{
    sprintf(text,
        "<h1>Lost Signal:</h1><p>"
        "<p style=\"text-indent: 4em;\">pitch             = %d</p>"
        "<p style=\"text-indent: 4em;\">duration          = %d</p>"
        "<p style=\"text-indent: 4em;\">frequency_seconds = %d</p></p>",
        cfg.lost_signal.pitch,
        cfg.lost_signal.duration,
        cfg.lost_signal.frequency_seconds);

    return text;
}
//------------------------------------------------------
static void HTTP_handleSettingsLostSIgnal()
{
    NYG::CarConfig temp = cfg;

    long N;

    #define GET_LOST_SIGNAL_INT_PARAM(fld, min, max)      GET_NUMERIC_PARAM(Int, fld, min, max, N, lost_signal)

    GET_LOST_SIGNAL_INT_PARAM(pitch, 0, 10000);
    GET_LOST_SIGNAL_INT_PARAM(duration, 10, 1000);
    GET_LOST_SIGNAL_INT_PARAM(frequency_seconds, 2, 60);

    double D;

    GET_NUMERIC_PARAM(Real, factor, 1, 5, D, motor);

    update_cfg(temp);
    
    char text[MAX_SETTINGS_TEXT_SIZE];
    server.send(200, "text/html", get_lost_signal_settings(text));
}
//------------------------------------------------------
static void HTTP_handleSettingsView()
{
    char text[MAX_SETTINGS_TEXT_SIZE * 2] = { 0 };
    char temp[MAX_SETTINGS_TEXT_SIZE];
    strcpy(text, get_sirena_settings(temp));
    strcat(text, get_motor_settings(temp));
    strcat(text, get_lost_signal_settings(temp));

    server.send(200, "text/html", text);
}
//------------------------------------------------------

//------------------------------------------------------
//	HTTP answers
//------------------------------------------------------

static void send_OK()
{
    //LOGGER << "send_OK" << NL;
    server.send(200, "text/html", "OK");
}
//------------------------------------------------------
static void send_BadRequest()
{
    LOGGER << "send_BadRequest: " << server.uri() << NL;
    server.send(400, "text/html", "Bad request");
}
//------------------------------------------------------

NYG::CarConfig   cfg  = 
{
    "Noami'sCar",                       //    char    name[16];
    0,                                  //    uint8_t version;

                                        //    struct
    {
        true,                           //      bool        use;
        50,                             //      uint8_t     duration;   // = 50
    },                                  //    sirena;

                                        //    struct
    {
        40,                             //      uint16_t    start_moving_threshold,     // =  40
        100,                            //                  min_speed,                  // = 100
        400,                            //                  max_speed_slow,             // = 400
        1000,                           //                  max_speed_fast;             // = 1000
        2.0,                            //      double      factor;
    },                                  //    motor;

                                        //    struct
    {
        800,                            //      uint16_t    pitch;                      // = 800
        100,                            //      uint16_t    duration;                   // = 100
        30,                             //      uint16_t    frequency_seconds;          // = 30
    }                                   //    lost_signal;
};
