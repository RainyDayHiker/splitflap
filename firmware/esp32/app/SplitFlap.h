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
#pragma once

#include <Arduino.h>

#include "../core/logger.h"
#include "../core/splitflap_task.h"
#include "../core/task.h"

class SimpleWebServer; // Forward declaration

class SplitFlap : public Task<SplitFlap>
{
	friend class Task<SplitFlap>; // Allow base Task to invoke protected run()

public:
	SplitFlap(SplitflapTask &splitflapTask, Logger &logger, const uint8_t task_core = 0);

	// Register web handlers related to splitflap operations
	void registerHandlers(SimpleWebServer &webServer);

protected:
	void run();

private:
	SplitflapTask &splitFlap;
	Logger &logger;

	// Builds the JSON state string for /splitflap/state.json
	String buildStateJson();

	// Handler for POST /splitflap/set_flap
	void handleSetFlap(SimpleWebServer &webServer);

	// Handler for POST /splitflap/set_flap_state
	void handleSetFlapState(SimpleWebServer &webServer);

	// Handler for POST /splitflap/set_offsets
	void handleSetOffsets(SimpleWebServer &webServer);
};
