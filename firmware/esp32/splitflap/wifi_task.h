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
#include <WiFi.h>
#include <lwip/apps/sntp.h>
#include <time.h>

#include "../core/logger.h"
#include "../core/task.h"
#include "display_task.h"

enum class WiFiState
{
	DISCONNECTED,
	CONNECTING,
	CONNECTED,
	TIME_SYNCING,
	READY,
	ERROR
};

class WiFiTask : public Task<WiFiTask>
{
	friend class Task<WiFiTask>; // Allow base Task to invoke protected run()

public:
	WiFiTask(DisplayTask &display_task, Logger &logger, const uint8_t task_core);

	// Check if WiFi is connected and time is synced
	bool isReady() const;

	// Check if WiFi is connected (but time may not be synced yet)
	bool isConnected() const;

	// Get current WiFi state
	WiFiState getState() const;

	// Get WiFi status string for display
	String getStatusString() const;

protected:
	void run();

private:
	void connectWifi();
	void syncTime();
	void updateState();
	void updateDisplayStatus();

	DisplayTask &display_task_;
	Logger &logger_;

	WiFiState state_;
	uint32_t last_connection_attempt_;
	uint32_t last_status_update_;
	bool time_synced_;

	static const uint32_t CONNECTION_RETRY_INTERVAL = 10000; // 10 seconds
	static const uint32_t STATUS_UPDATE_INTERVAL = 5000;	 // 5 seconds
};
