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
#if HTTP
#include "http_task.h"

#include <HTTPClient.h>
#include <json11.hpp>
#include <time.h>

using namespace json11;

// About this example:
// - Fetches current weather data for an area in San Francisco (updating infrequently)
// - Cycles between showing the temperature and the wind speed on the split-flaps (cycles frequently)
//
// Make sure to set up secrets.h - see secrets.h.example for more.
//
// What this example demonstrates:
// - a simple JSON GET request (see fetchData)
// - json response parsing using json11 (see handleData)
// - cycling through messages at a different interval than data is loaded (see run)

// Update data every 10 minutes
#define REQUEST_INTERVAL_MILLIS (10 * 60 * 1000)

// Cycle the message that's showing more frequently, every 30 seconds (exaggerated for example purposes)
#define MESSAGE_CYCLE_INTERVAL_MILLIS (4 * 1000)

// Don't show stale data if it's been too long since successful data load
#define STALE_TIME_MILLIS (REQUEST_INTERVAL_MILLIS * 3)

// Public token for synoptic data api (it's not secret, but please don't abuse it)
#define SYNOPTICDATA_TOKEN "e763d68537d9498a90fa808eb9d415d9"

bool HTTPTask::fetchData()
{
	char buf[200];
	uint32_t start = millis();
	HTTPClient http;

	return false;
	// Construct the http request
	http.begin("https://api.synopticdata.com/v2/stations/latest?&token=" SYNOPTICDATA_TOKEN "&within=30&units=english&vars=air_temp,wind_speed&varsoperator=and&radius=37.765157,-122.419702,4&limit=20&fields=stid");

	// If you wanted to add headers, you would do so like this:
	// http.addHeader("Accept", "application/json");

	// Send the request as a GET
	logger_.log("Sending request");
	int http_code = http.GET();

	snprintf(buf, sizeof(buf), "Finished request in %lu millis.", millis() - start);
	logger_.log(buf);
	if (http_code > 0)
	{
		String data = http.getString();
		http.end();

		snprintf(buf, sizeof(buf), "Response code: %d Data length: %d", http_code, data.length());
		logger_.log(buf);

		std::string err;
		Json json = Json::parse(data.c_str(), err);

		if (err.empty())
		{
			logger_.log(data.c_str());
			return handleData(json);
		}
		else
		{
			snprintf(buf, sizeof(buf), "Error parsing response! %s", err.c_str());
			logger_.log(buf);
			return false;
		}
	}
	else
	{
		snprintf(buf, sizeof(buf), "Error on HTTP request (%d): %s", http_code, http.errorToString(http_code).c_str());
		logger_.log(buf);
		http.end();
		return false;
	}
}

bool HTTPTask::handleData(Json json)
{
	// Extract data from the json response. You could use ArduinoJson, but I find json11 to be much
	// easier to use albeit not optimized for a microcontroller.

	// Example data:
	/*
		{
			...
			"STATION": [
				{
					"STID": "F4637",
					"OBSERVATIONS": {
						"wind_speed_value_1": {
							"date_time": "2021-11-30T23:25:00Z",
							"value": 0.87
						},
						"air_temp_value_1": {
							"date_time": "2021-11-30T23:25:00Z",
							"value": 69
						}
					},
					...
				},
				{
					"STID": "C5988",
					"OBSERVATIONS": {
						"wind_speed_value_1": {
							"date_time": "2021-11-30T23:24:00Z",
							"value": 1.74
						},
						"air_temp_value_1": {
							"date_time": "2021-11-30T23:24:00Z",
							"value": 68
						}
					},
					...
				},
				...
			]
		}
	*/

	// Validate json structure and extract data:
	auto station = json["STATION"];
	if (!station.is_array())
	{
		logger_.log("Parse error: STATION");
		return false;
	}
	auto station_array = station.array_items();

	std::vector<double> temps;
	std::vector<double> wind_speeds;

	for (uint8_t i = 0; i < station_array.size(); i++)
	{
		auto item = station_array[i];
		if (!item.is_object())
		{
			logger_.log("Bad station item, ignoring");
			continue;
		}
		auto observations = item["OBSERVATIONS"];
		if (!observations.is_object())
		{
			logger_.log("Bad station observations, ignoring");
			continue;
		}

		auto air_temp_value = observations["air_temp_value_1"];
		if (!air_temp_value.is_object())
		{
			logger_.log("Bad air_temp_value_1, ignoring");
			continue;
		}
		auto value = air_temp_value["value"];
		if (!value.is_number())
		{
			logger_.log("Bad air temp, ignoring");
			continue;
		}
		temps.push_back(value.number_value());

		auto wind_speed_value = observations["wind_speed_value_1"];
		if (!wind_speed_value.is_object())
		{
			logger_.log("Bad wind_speed_value_1, ignoring");
			continue;
		}
		value = wind_speed_value["value"];
		if (!value.is_number())
		{
			logger_.log("Bad wind speed, ignoring");
			continue;
		}
		wind_speeds.push_back(value.number_value());
	}

	auto entries = temps.size();
	if (entries == 0)
	{
		logger_.log("No data found");
		return false;
	}

	// Calculate medians
	std::sort(temps.begin(), temps.end());
	std::sort(wind_speeds.begin(), wind_speeds.end());
	double median_temp;
	double median_wind_speed;
	if ((entries % 2) == 0)
	{
		median_temp = (temps[entries / 2 - 1] + temps[entries / 2]) / 2;
		median_wind_speed = (wind_speeds[entries / 2 - 1] + wind_speeds[entries / 2]) / 2;
	}
	else
	{
		median_temp = temps[entries / 2];
		median_wind_speed = wind_speeds[entries / 2];
	}

	char buf[200];
	snprintf(buf, sizeof(buf), "Medians from %d stations: temp=%dºF, wind speed=%d knots", entries, (int)median_temp, (int)median_wind_speed);
	logger_.log(buf);

	// Construct the messages to display
	messages_.clear();

	snprintf(buf, sizeof(buf), "%d f", (int)median_temp);
	messages_.push_back(String(buf));

	snprintf(buf, sizeof(buf), "%d mph", (int)(median_wind_speed * 1.151));
	messages_.push_back(String(buf));

	// Show the data fetch time on the LCD
	time_t now;
	time(&now);
	strftime(buf, sizeof(buf), "Data: %Y-%m-%d %H:%M:%S", localtime(&now));
	display_task_.setMessage(0, String(buf));
	return true;
}

HTTPTask::HTTPTask(SplitflapTask &splitflap_task, DisplayTask &display_task, WiFiTask &wifi_task, Logger &logger, const uint8_t task_core) : Task("HTTP", 8192, 1, task_core),
																																			 splitflap_task_(splitflap_task),
																																			 display_task_(display_task),
																																			 wifi_task_(wifi_task),
																																			 logger_(logger),
																																			 wifi_client_()
{
}

void HTTPTask::run()
{
	char buf[max(NUM_MODULES + 1, 200)];
	char character_list[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZa0123456789b.?-$'#,!&cdef";

	// Wait for WiFi to be ready (connected and time synced)
	while (!wifi_task_.isReady())
	{
		logger_.log("HTTP: Waiting for WiFi to be ready...");
		delay(5000);
	}

	logger_.log("HTTP: WiFi is ready, starting HTTP task");

	bool stale = false;
	while (1)
	{
		// Check if WiFi is still ready, if not wait for it
		if (!wifi_task_.isReady())
		{
			logger_.log("HTTP: WiFi not ready, waiting...");
			delay(5000);
			continue;
		}

		long now = millis();

		bool update = false;
		if (http_last_request_time_ == 0 || now - http_last_request_time_ > REQUEST_INTERVAL_MILLIS)
		{
			if (fetchData())
			{
				http_last_success_time_ = millis();
				stale = false;
				update = true;
			}
			http_last_request_time_ = millis();
		}

		if (!stale && http_last_success_time_ > 0 && millis() - http_last_success_time_ > STALE_TIME_MILLIS)
		{
			stale = true;
			messages_.clear();
			messages_.push_back("stale");
			update = true;
		}

		if (update || now - last_message_change_time_ > MESSAGE_CYCLE_INTERVAL_MILLIS)
		{
			if (current_message_index_ >= 4)
			{
				current_message_index_ = 0;
			}

			if (current_message_index_ == 0)
				snprintf(buf, sizeof(buf), "bHELLO");
			else if (current_message_index_ == 1)
				snprintf(buf, sizeof(buf), "WORLD!");
			else if (current_message_index_ == 2)
				snprintf(buf, sizeof(buf), "eSPLIT");
			else if (current_message_index_ == 3)
				snprintf(buf, sizeof(buf), "FLAPfa");

			// char logbuf[256];
			// snprintf(logbuf, sizeof(logbuf), "Setting splitflap to %s", buf);
			// logger_.log(logbuf);

			// // Pad message for display
			// size_t len = 2;
			// memset(buf + len, ' ', sizeof(buf) - len);

			// splitflap_task_.showString(buf, NUM_MODULES, false);

			current_message_index_++;
			last_message_change_time_ = millis();
		}

		delay(1000);
	}
}
#endif
