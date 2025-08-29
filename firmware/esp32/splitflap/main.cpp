/*
   Copyright 2020 Scott Bezek and the splitflap contributors

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

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <LITTLEFS.h>

#include "config.h"

#include "../core/configuration.h"
#include "../core/splitflap_task.h"
#include "debug_build_info.h"
#include "display_task.h"
#include "serial_task.h"

#if ENABLE_DISPLAY || HTTP || MQTT
#include "wifi_task.h"
#endif

Configuration config;

SplitflapTask splitflapTask(1, LedMode::AUTO);
SerialTask serialTask(splitflapTask, 0);

#if ENABLE_DISPLAY
DisplayTask displayTask(splitflapTask, 0);
#endif

#if ENABLE_DISPLAY || HTTP || MQTT
WiFiTask wifiTask(displayTask, serialTask, 0);
#endif

#ifdef CHAINLINK_BASE
#include "../base/base_supervisor_task.h"
BaseSupervisorTask baseSupervisorTask(splitflapTask, serialTask, 0);
#endif

#if MQTT
#include "mqtt_task.h"
MQTTTask mqttTask(splitflapTask, displayTask, serialTask, 0);
#endif

#if HTTP
#include "http_task.h"
HTTPTask httpTask(splitflapTask, displayTask, wifiTask, serialTask, 0);
#endif

void setup()
{
	serialTask.begin();

	if (!LITTLEFS.begin(true /*FORMAT_LITTLEFS_IF_FAILED*/))
	{
		Serial.println("LITTLEFS Mount Failed");
		return;
	}

	config.setLogger(&serialTask);
	bool loaded = config.loadFromDisk();

	splitflapTask.begin();
	splitflapTask.setConfiguration(&config);

	if (loaded)
	{
		PB_PersistentConfiguration saved = config.get();
		uint16_t offsets[NUM_MODULES] = {};
		for (uint8_t i = 0; i < min(saved.module_offset_steps_count, (pb_size_t)NUM_MODULES); i++)
		{
			offsets[i] = saved.module_offset_steps[i];
		}
		splitflapTask.restoreAllOffsets(offsets);
	}

#if ENABLE_DISPLAY
	displayTask.begin();
#endif

#if ENABLE_DISPLAY || HTTP || MQTT
	wifiTask.begin();
#endif

#if MQTT
	mqttTask.begin();
#endif

#if HTTP
	httpTask.begin();
#endif

#ifdef CHAINLINK_BASE
	baseSupervisorTask.begin();
#endif

	logDebugBuildInfo(serialTask);

	// Delete the default Arduino loopTask to free up Core 1
	vTaskDelete(NULL);
}

void loop()
{
	assert(false);
}
