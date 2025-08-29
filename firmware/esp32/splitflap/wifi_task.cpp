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

#include "wifi_task.h"
#include "secrets.h"

WiFiTask::WiFiTask(DisplayTask &display_task, Logger &logger, const uint8_t task_core)
	: Task("WiFi", 4096, 1, task_core),
	  display_task_(display_task),
	  logger_(logger),
	  state_(WiFiState::DISCONNECTED),
	  last_connection_attempt_(0),
	  last_status_update_(0)
{
}

bool WiFiTask::isReady() const
{
	return state_ == WiFiState::READY;
}

bool WiFiTask::isConnected() const
{
	return WiFi.status() == WL_CONNECTED;
}

WiFiState WiFiTask::getState() const
{
	return state_;
}

String WiFiTask::getStatusString() const
{
	switch (WiFi.status())
	{
	case WL_IDLE_STATUS:
		return "WiFi: Idle";
	case WL_NO_SSID_AVAIL:
		return "WiFi: No SSID";
	case WL_CONNECTED:
		return String("WiFi: ") + String(WIFI_SSID) + " " + WiFi.localIP().toString();
	case WL_CONNECT_FAILED:
		return "WiFi: Connection failed";
	case WL_CONNECTION_LOST:
		return "WiFi: Connection lost";
	case WL_DISCONNECTED:
		return "WiFi: Disconnected";
	default:
		return "WiFi: Unknown";
	}
}

void WiFiTask::run()
{
	while (true)
	{
		updateState();
		updateDisplayStatus();

		uint32_t now = millis();

		switch (state_)
		{
		case WiFiState::DISCONNECTED:
			if (now - last_connection_attempt_ > CONNECTION_RETRY_INTERVAL)
			{
				connectWifi();
				last_connection_attempt_ = now;
			}
			break;

		case WiFiState::CONNECTING:
			if (WiFi.status() == WL_CONNECTED)
			{
				char buf[256];
				snprintf(buf, sizeof(buf), "Connected to network %s", WIFI_SSID);
				logger_.log(buf);
				state_ = WiFiState::CONNECTED;
			}
			else if (now - last_connection_attempt_ > CONNECTION_RETRY_INTERVAL)
			{
				logger_.log("WiFi connection timeout, retrying...");
				state_ = WiFiState::DISCONNECTED;
			}
			break;

		case WiFiState::CONNECTED:
			if (WiFi.status() != WL_CONNECTED)
			{
				logger_.log("WiFi connection lost");
				state_ = WiFiState::DISCONNECTED;
			}
			break;

		case WiFiState::READY:
			if (WiFi.status() != WL_CONNECTED)
			{
				logger_.log("WiFi connection lost");
				state_ = WiFiState::DISCONNECTED;
			}
			break;

		case WiFiState::ERROR:
			// Wait a bit longer before retrying from error state
			if (now - last_connection_attempt_ > CONNECTION_RETRY_INTERVAL * 2)
			{
				state_ = WiFiState::DISCONNECTED;
			}
			break;
		}

		delay(1000); // Check WiFi status every second
	}
}

void WiFiTask::connectWifi()
{
	state_ = WiFiState::CONNECTING;

	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

	// Disable WiFi sleep as it causes glitches on pin 39;
	// see https://github.com/espressif/arduino-esp32/issues/4903#issuecomment-793187707
	WiFi.setSleep(WIFI_PS_NONE);

	logger_.log("Establishing connection to WiFi..");

	char buf[256];
	snprintf(buf, sizeof(buf), "WiFi connecting to %s", WIFI_SSID);
	display_task_.setMessage(1, String(buf));
}

void WiFiTask::updateState()
{
	// This method can be used for any additional state updates in the future
}

void WiFiTask::updateDisplayStatus()
{
	uint32_t now = millis();
	if (now - last_status_update_ > STATUS_UPDATE_INTERVAL)
	{
		// Update display with current status less frequently to avoid spam
		display_task_.setMessage(1, getStatusString());
		last_status_update_ = now;
	}
}
