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
#include "SplitFlap.h"

#include "LocalTime.h"
#include "Config.h"
#include "SimpleWebServer.h"
#include <ArduinoJson.h>
#include "../splitflap/display_layouts.h"

SplitFlap::SplitFlap(SplitflapTask &splitflapTask, Logger &logger, const uint8_t task_core) : Task("SplitFlap", 8192, 1, task_core),
																							  splitFlap(splitflapTask),
																							  logger(logger)
{
}

void SplitFlap::run()
{
	while (1)
	{
		delay(120 * 1000); // Nothing to do so sleep a lot
	}
}

void SplitFlap::registerHandlers(SimpleWebServer &webServer)
{
	webServer.AddHandler("/splitflap/state.json", [this, &webServer]()
						 {
      String json = buildStateJson();
      webServer.RespondWithContent(200, json); });
}

String SplitFlap::buildStateJson()
{
	SplitflapState state = splitFlap.getState();
	JsonDocument doc;

	doc["mode"] = (state.mode == SplitflapMode::MODE_SENSOR_TEST) ? "sensor_test" : "run";
#ifdef CHAINLINK
	doc["loopbacks_ok"] = state.loopbacks_ok;
#endif

	// Layout metadata
	doc["num_modules"] = NUM_MODULES;
	doc["display_columns"] = DISPLAY_COLUMNS;
	uint16_t display_rows = (NUM_MODULES + DISPLAY_COLUMNS - 1) / DISPLAY_COLUMNS;
	doc["display_rows"] = display_rows;

	// Flap metadata
	doc["num_flaps"] = NUM_FLAPS;
	JsonArray flapsArray = doc["flaps"].to<JsonArray>();
	for (uint8_t i = 0; i < NUM_FLAPS; i++)
	{
		char c = (char)flaps[i];
		// For JSON, we can just add as a one-character string
		char buf[2] = {c, '\0'};
		flapsArray.add(buf);
	}

	JsonArray modules = doc["modules"].to<JsonArray>();
	for (uint8_t i = 0; i < NUM_MODULES; i++)
	{
		JsonObject m = modules.add<JsonObject>();
		m["index"] = i;
		m["state"] = (uint8_t)state.modules[i].state;
		m["flap_index"] = state.modules[i].flap_index;
		m["offset"] = state.modules[i].offset;
		m["moving"] = state.modules[i].moving;
		m["home"] = state.modules[i].home_state;
		m["count_unexpected_home"] = state.modules[i].count_unexpected_home;
		m["count_missed_home"] = state.modules[i].count_missed_home;

		uint8_t row = 0, col = 0;
		getLayoutPosition(i, &row, &col);
		m["row"] = row;
		m["col"] = col;
	}

	String json;
	serializeJson(doc, json);
	return json;
}
