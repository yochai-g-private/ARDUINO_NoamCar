// TestSpeedCalculation.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <math.h>       /* asin */

static void test(int X, int Y);
static void test(double dregrees);
int main()
{
	//test(500, 0);
	//test(500, 500);
	//test(0, 500);
	//test(-500, 500);
	//test(-500, 0);
	//test(-500, -500);
	//test(0, -500);
	//test(500, -500);

	int degrees;

	#define STEP	1

	//test(38);
	//test(37);

#if 1
	for (degrees = 90; degrees >= 0; degrees -= STEP)
	{
		test(degrees);
	}

	for (degrees = 345; degrees >= 90; degrees -= STEP)
	{
		test(degrees);
	}
#endif

	char s[100];
	gets_s(s);
}

void CalculateSpeed_Old(double X, double Y, int16_t& right_speed, int16_t& left_speed);
void CalculateSpeed(double X, double Y, int16_t& right_speed, int16_t& left_speed);

static void test(int X, int Y)
{
	int16_t right_speed, left_speed;

	CalculateSpeed(X, Y, right_speed, left_speed);
	printf("%4d * %4d ==> %4d * %4d\n", X, Y, right_speed, left_speed);
	//CalculateSpeed_Old(X, Y, right_speed, left_speed);
	//printf("%4d * %4d ==> %4d * %4d\n", X, Y, right_speed, left_speed);
	printf("-----------------------------------------------\n");
}

static void test(int degrees, int X, int Y)
{
	int16_t right_speed, left_speed;

	CalculateSpeed(X, Y, right_speed, left_speed);
	printf("%3d: %4d * %4d ==> %4d * %4d : %4d : %lf\n", degrees, X, Y, left_speed, right_speed, left_speed - right_speed, (double)right_speed / (double)left_speed);
	//CalculateSpeed_Old(X, Y, right_speed, left_speed);
	//printf("     %4d * %4d ==> %4d * %4d\n", X, Y, right_speed, left_speed);
	//printf("-----------------------------------------------\n");
}

#define PI 3.14159265

static void test(double degrees)
{
	double radians = degrees * PI / 180;
	static double RADIUS = 500;
	double sin_val = sin(radians);
	//printf("%lf ==> ", sin_val);
	double X = sin_val * RADIUS;
	double Y = sqrt(RADIUS*RADIUS - X * X);

	if (degrees > 90 && degrees < 270)
		Y = -Y;

	test(degrees, X, Y);
}