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
#include "clock_task.h"

#include "LocalTime.h"

Clock::Clock(SplitflapTask &splitflap_task, Logger &logger, const uint8_t task_core) : Task("Clock", 8192, 1, task_core),
																					   splitflap_task_(splitflap_task),
																					   logger_(logger)
{
}

void Clock::run()
{
	char buf[max(NUM_MODULES + 1, 200)];
	char character_list[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZa0123456789b.?-$'#,!&cdef";

	while (1)
	{
		// Check to see if we have a good time sync
		if (!LocalTime::IsTimeCloseToDefaultTime())
		{
			struct tm timeinfo;
			LocalTime::GetCurrentTime(&timeinfo);

			// No updates at night
			if (timeinfo.tm_hour > 7 && timeinfo.tm_hour < 22)
			{
				char temp[6];
				strftime(temp, sizeof(temp), "%H:%M", &timeinfo);
				if (!current_time_.equals(temp))
				{
					current_time_ = temp;

					// Update flaps - for now, we're going to go with time + an emoji for the day of week for testing
					char emoji = 'a' + timeinfo.tm_wday;
					if (emoji == 'g')
						emoji = '!';
					snprintf(buf, sizeof(buf), "%s%c", temp, emoji);
					logger_.logf("Clock: updating time to %s", buf);
					splitflap_task_.showString(buf, NUM_MODULES, false);
					delay(55 * 1000);
				}
				else
					delay(500);
			}
			else
				delay(60 * 1000);
		}
		else
			delay(1000);
	}
}
