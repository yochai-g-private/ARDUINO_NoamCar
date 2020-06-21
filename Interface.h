#pragma once

#define WIFI_SSID			"Noami'sTJ"

#define _DEGUGGING	1

#if _DEGUGGING
#define WIFI_PASSCODE		NULL
#define WIFI_HIDDEN			false
#else
#define WIFI_PASSCODE		"Haifa2015"
#define WIFI_HIDDEN			true
#endif //_DEGUGGING

#define HOST_ADDRESS		"192.168.4.1"
#define HOST_PORT			80

#define URL_CAR_DATA		"/CarData/"

#define URL_PARAM_X			"X"
#define URL_PARAM_Y			"Y"
#define URL_PARAM_FAST		"F"
#define URL_PARAM_SIGNAL	"S"

#define SPEED_LOW           0
#define SPEED_HIGH          1

#define KEEP_ALIVE_TIMEOUT_SECONDS		5
#define KEEP_ALIVE_TIMEOUT_MILLIS		(KEEP_ALIVE_TIMEOUT_SECONDS * 1000)

namespace NYG
{
    enum SignalType
    {
        SIGNAL_NONE,        __SIGNAL_MIN__ = SIGNAL_NONE,
        SIGNAL_LIGHTS,
        SIGNAL_SOUND,       __SIGNAL_MAX__ = SIGNAL_SOUND,
    };

    struct CarData
    {
        int         X,
                    Y;
        bool        FAST;
        SignalType  SIGNAL;
    };

    struct CarConfig
    {
        char    name[16];
        uint8_t version;

        struct 
        {
            bool        use;
            uint16_t    duration;   // = 50
        }   sirena;

        struct
        {
            uint16_t    start_moving_threshold,     // =  40
                        min_speed,                  // = 100
                        max_speed_slow,             // = 400
                        max_speed_fast;             // = 1000
            double      factor;
        }   motor;

        struct
        {
            uint16_t    pitch;                      // = 800
            uint16_t    duration;                   // = 100
            uint16_t    frequency_seconds;          // = 30
        } lost_signal;
    };
}

extern NYG::CarConfig   cfg;
