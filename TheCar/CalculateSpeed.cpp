#include "NYG.h"
#include "Templates.h"
#include "..\Interface.h"

using namespace NYG;

enum
{
	MAX_COORDINATE			= 512,
	MAX_SPEED				= 1022,
	MIN_SPEED				= -MAX_SPEED,

#if 1
    #define DE_FACTO_MAX_SPEED_SLOW	    cfg.motor.max_speed_slow
    #define DE_FACTO_MAX_SPEED_FAST     cfg.motor.max_speed_fast
    #define DE_FACTO_MIN_SPEED          cfg.motor.min_speed
    #define START_MOVING_THRESHOLD      cfg.motor.start_moving_threshold
    #define FACTOR                      cfg.motor.factor

#else
	DE_FACTO_MAX_SPEED_SLOW	= 400,
	DE_FACTO_MAX_SPEED_FAST = 1000,
	DE_FACTO_MIN_SPEED		= 100,
	START_MOVING_THRESHOLD  = 40,

    #define FACTOR		    2.0

#endif
};

#if 1
    #define DE_FACTO_MAX_SPEED  ((fast) ? DE_FACTO_MAX_SPEED_FAST : DE_FACTO_MAX_SPEED_SLOW)
#else
    static int MaxSpeeds[] = { DE_FACTO_MAX_SPEED_SLOW, DE_FACTO_MAX_SPEED_FAST };
    #define DE_FACTO_MAX_SPEED	MaxSpeeds[fast]
#endif

void CalculateSpeed(double X, double Y, bool fast, int16_t& R_speed, int16_t& L_speed)
{
	if (abs(X) < START_MOVING_THRESHOLD)		X = 0;
	if (abs(Y) < START_MOVING_THRESHOLD)		Y = 0;

	if (X == 0 && Y == 0)
	{
		R_speed = 0;
		L_speed = 0;

		return;
	}

	X *= FACTOR;

	double L  = X + Y,
		   R  = X - Y;

	double abs_L = abs(L);
	double abs_R = abs(R);

#	define MapSpeed(SIDE)	\
		SIDE ## _speed = (int)MapEx<double>(abs_ ## SIDE, 0, (double)MAX_COORDINATE * FACTOR, DE_FACTO_MIN_SPEED, DE_FACTO_MAX_SPEED, 1);\
		if (abs_ ## SIDE != SIDE)	SIDE ## _speed = -SIDE ## _speed

	MapSpeed(R);
	MapSpeed(L);
}
