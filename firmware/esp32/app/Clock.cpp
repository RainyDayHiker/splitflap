/*
   Copyright 2021 Scott Bezek and the splitflap contributors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include "Clock.h"

#include "LocalTime.h"
#include "Config.h"

Clock::Clock(SplitflapTask &splitflapTask, Logger &logger, const uint8_t task_core) : Task("Clock", 8192, 1, task_core),
																					  splitFlap(splitflapTask),
																					  logger(logger)
{
}

void Clock::run()
{
	char buf[max(NUM_MODULES + 1, 200)];

	while (1)
	{
		// Check to see if we have a good time sync and a good config
		if (!LocalTime::HasTimeSyncHappened() || Config::GetInstance() == nullptr)
		{
			delay(1000);
			continue;
		}

		if (!Config::GetInstance()->GetAutoStatusUpdatesEnabled())
		{
			delay(5000);
			continue;
		}

		struct tm timeinfo;
		LocalTime::GetCurrentTime(&timeinfo);

		// No updates at night - ensure current time is not within the Quiet Time config checking against the hour and minute
		if (Config::GetInstance()->IsTimeInQuietPeriod(&timeinfo))
		{
			if (!currentTime.equals(""))
			{
				currentTime = "";
				logger.log("Clock: Nighttime, clearing display");
				for (int i = 0; i < NUM_MODULES; i++)
					buf[i] = ' ';
				splitFlap.showString(buf, NUM_MODULES, false);
			}
			delay(60 * 1000);
			continue;
		}

		// Calc the time that should be shown
		char temp[6];
		strftime(temp, sizeof(temp), "%H:%M", &timeinfo);

		if (currentTime.equals(temp))
		{
			// No change in time, do nothing
			delay(500);
			continue;
		}

		currentTime = temp;

		// Update flaps - for now, we're going to go with time + an emoji for the day of week for testing
		char emoji = 'a' + timeinfo.tm_wday;
		if (emoji == 'g')
			emoji = '!';
		snprintf(buf, sizeof(buf), "abcdef%s%cabcdefabcdef", temp, emoji);
		logger.logf("Clock: updating time to %s", buf);
		splitFlap.showString(buf, NUM_MODULES, false);
		delay(55 * 1000);
	}
}
