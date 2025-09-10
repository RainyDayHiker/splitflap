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

	static bool HasTimeSyncHappened();

	static char *DateTimeString(char *buffer, size_t size, time_t time);

	static int CompareTime(const struct tm *time, const int hour, const int minute)
	{
		if (time->tm_hour < hour)
			return -1;
		else if (time->tm_hour > hour)
			return 1;
		else if (time->tm_min < minute)
			return -1;
		else if (time->tm_min > minute)
			return 1;
		return 0;
	}
private:
};