#pragma once
#include <cstddef>
#include <ctime>

class LocalTime
{
public:
	typedef enum
	{
		Universal = 0,
		NewYork = 1,
		Chicago = 2,
		Denver = 3,
		LosAngeles = 4
	} TimeZone;

	static void setupTime(TimeZone tz);

	static void UpdateTimeZone(TimeZone tz);

	static time_t GetCurrentTime(struct tm *timeLocal, struct tm *timeUTC = nullptr);

	static bool IsTimeCloseToDefaultTime();

	static char *DateTimeString(char *buffer, size_t size, time_t time);

private:
};