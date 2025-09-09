#include <Arduino.h>
#include <time.h>
#include <sys/time.h>
// #include <TZ.h>

#include "LocalTime.h"

// OPTIONAL: change SNTP startup delay
// a weak function is already defined and returns 0 (RFC violation)
// it can be redefined:
uint32_t sntp_startup_delay_MS_rfc_not_less_than_60000()
{
	// info_sntp_startup_delay_MS_rfc_not_less_than_60000_has_been_called = true;
	// return 60000; // 60s (or lwIP's original default: (random() % 5000))
	return 10000; // 10s - RFC violation, but I want my device to start faster.
}

// OPTIONAL: change SNTP update delay
// a weak function is already defined and returns 1 hour
// it can be redefined:
// uint32_t sntp_update_delay_MS_rfc_not_less_than_15000 ()
//{
//    //info_sntp_update_delay_MS_rfc_not_less_than_15000_has_been_called = true;
//    return 15000; // 15s
//}

// Time that will be the default starting time (GMT): Sunday, January 1, 2020 8:00:00 AM
#define INITIAL_TIME 1577865600

// Forward declaration to make VS Code intellisense happy.
int settimeofday(const struct timeval *tv, const struct timezone *tz);
extern "C" int _gettimeofday_r(struct _reent *unused, struct timeval *tp, void *tzp);

struct TimeZoneInfo
{
	LocalTime::TimeZone tz;
	const char *timezoneInfo;
};

// Info from TZ.h
static TimeZoneInfo timeZones[] = {{LocalTime::TimeZone::Universal, "UTC0"},				 // TZ_Etc_Universal
								   {LocalTime::TimeZone::NewYork, "EST5EDT,M3.2.0,M11.1.0"}, // TZ_America_New_York
								   {LocalTime::TimeZone::Chicago, "CST6CDT,M3.2.0,M11.1.0"},
								   {LocalTime::TimeZone::Denver, "MST7MDT,M3.2.0,M11.1.0"},
								   {LocalTime::TimeZone::LosAngeles, "PST8PDT,M3.2.0,M11.1.0"}};
const int timezoneCount = sizeof(timeZones) / sizeof(TimeZoneInfo);

static const char *GetTimeZoneInfo(LocalTime::TimeZone tz)
{
	for (int i = 0; i < timezoneCount; i++)
		if (timeZones[i].tz == tz)
			return timeZones[i].timezoneInfo;
	return timeZones[0].timezoneInfo; // return default if not found
}

void LocalTime::setupTime(TimeZone tz)
{
	// Setup an inital time before we get it from the network
	time_t rtc = INITIAL_TIME;
	timeval tv = {rtc, 0};
	settimeofday(&tv, nullptr);

	// Configure the NTP time
	configTzTime(GetTimeZoneInfo(tz), "pool.ntp.org", "time.nist.gov");
}

void LocalTime::UpdateTimeZone(TimeZone tz)
{
	configTzTime(GetTimeZoneInfo(tz), "pool.ntp.org", "time.nist.gov");
}

time_t LocalTime::GetCurrentTime(tm *timeLocal, tm *timeUTC)
{
	time_t now = time(nullptr);
	if (timeLocal != nullptr)
		localtime_r(&now, timeLocal);
	if (timeUTC != nullptr)
		gmtime_r(&now, timeUTC);
	return now;
}

bool LocalTime::IsTimeCloseToDefaultTime()
{
	time_t now = time(nullptr);

	// difftime returns diff in seconds - compare to a year.  If the device is running that long without getting a real time, chaos is fine.
	return (difftime(now, INITIAL_TIME) < (60 * 60 * 24 * 365));
}

char *LocalTime::DateTimeString(char *buffer, size_t size, time_t time)
{
	struct tm tempTime;
	localtime_r(&time, &tempTime);
	strftime(buffer, size, "%m/%d/%Y %H:%M:%S", &tempTime);
	return buffer;
}
