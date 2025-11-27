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
#include "../splitflap/display_task.h"
#include "SimpleWebServer.h"

enum class WiFiState
{
	DISCONNECTED,
	CONNECTING,
	CONNECTED,
	READY,
	ERROR
};

// Forward declare to make interconnected headers happy
class SimpleWebServer;

class Network : public Task<Network>
{
	friend class Task<Network>; // Allow base Task to invoke protected run()

public:
	Network(DisplayTask &display_task, SimpleWebServer &webServer, Logger &logger, const uint8_t task_core = 0);

	bool HandleCaptivePortal(String serverHostname);

	void Setup();

protected:
	void run();

private:
	void ConfigModeStart();
	void ConfigModeStop(String ssid = "", String password = "");
	void UpdateSSID(String ssid, String password);

	void RedirectToConfig();

	void HandleWifiSetup();

	void HandleConfigPage();
	void UpdateNetworkSettings();
	void GetNetworkList();

	void updateDisplayStatus();
	void ensureMDNS();
	void stopMDNS();

	DisplayTask &displayTask;
	SimpleWebServer &webServer;
	Logger &logger;

	bool inConfigMode;
	bool attemptingNewSSID;
	String hostNameFQDN;
	String apHostName;

	String lastStatusSSID;
	wl_status_t lastWifiStatus;
	bool mdnsStarted;
};
