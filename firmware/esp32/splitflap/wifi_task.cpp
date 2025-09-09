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
#include <ESPmDNS.h>
#include <DNSServer.h>

#define ESP_DRD_USE_LITTLEFS true
#include <ESP_DoubleResetDetector.h>
#include <esp_wifi.h>

// Default to no SSID or PASSWORD but they can be overridden by build options
#ifndef WIFI_SSID
const String wifiSSID = "";
#else
const String wifiSSID = WIFI_SSID;
#endif
#ifndef WIFI_PASSWORD
const String wifiPassword = "";
#else
const String wifiPassword = WIFI_PASSWORD;
#endif

boolean isIp(String str)
{
	for (size_t i = 0; i < str.length(); i++)
	{
		int c = str.charAt(i);
		if (c != '.' && (c < '0' || c > '9'))
		{
			return false;
		}
	}
	return true;
}

DNSServer *dnsServer;
DoubleResetDetector *doubleResetDetector;
uint32_t configModeStartTime = 0;

WiFiTask::WiFiTask(DisplayTask &display_task, WebServerTask &web_server_task, Logger &logger, const uint8_t task_core)
	: Task("WiFi", 4096, 1, task_core),
	  display_task_(display_task),
	  web_server_task_(web_server_task),
	  logger_(logger),
	  last_status_ssid("")
{
	_hostNameFQDN = DEVICE_INSTANCE_NAME;
	_hostNameFQDN += ".local";
	_hostNameFQDN.toLowerCase();

	_apHostName = DEVICE_INSTANCE_NAME;
	_apHostName += String((long)ESP.getEfuseMac(), HEX);
}

void WiFiTask::Setup()
{
	doubleResetDetector = new DoubleResetDetector(5 /* timeout in secs */, 0 /* RTC Memory address*/);

	WiFi.persistent(true);
	WiFi.setAutoConnect(true);
	WiFi.mode(WIFI_STA);
	// Disable WiFi sleep as it causes glitches on pin 39;
	// see https://github.com/espressif/arduino-esp32/issues/4903#issuecomment-793187707
	WiFi.setSleep(WIFI_PS_NONE);

	if (doubleResetDetector->detectDoubleReset())
	{
		// Double reset occurred, start config mode
		// This can be used to force the device into AP mode for configuration even with cached credentials
		logger_.log("wifi: Double reset detected");
		ConfigModeStart();
	}
	else
	{
		// check for cached WiFi credentials
		wifi_config_t cachedConfig;
		esp_err_t configResult = esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &cachedConfig);
		if (configResult == ESP_OK && strlen((const char *)cachedConfig.ap.ssid) > 0)
		{
			logger_.logf("wifi: cached SSID: %s found, connecting", cachedConfig.ap.ssid);
			WiFi.begin();
		}
		else if (wifiSSID != "" && wifiPassword != "")
		{
			logger_.logf("wifi: Attempting to use build provided ssid (%s) and password", wifiSSID.c_str());
			WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
		}
		else
		{
			// No creds, so start in AP mode
			ConfigModeStart();
		}
	}

	// Setup MDNS
	MDNS.begin(DEVICE_INSTANCE_NAME);
	MDNS.addService("http", "tcp", 80);

	// Register Page Handlers for the WebServer
	// Wifi setup
	web_server_task_.AddHandler("/network/config", std::bind(&WiFiTask::HandleConfigPage, this));
	web_server_task_.AddHandler("/network/network_update", std::bind(&WiFiTask::UpdateNetworkSettings, this));
	web_server_task_.AddHandler("/network/networks", std::bind(&WiFiTask::GetNetworkList, this));

	// Handle the various wifi config landing pages
	web_server_task_.AddHandler("/generate_204", std::bind(&WiFiTask::HandleWifiSetup, this));		  // Android wifi portal.
	web_server_task_.AddHandler("/fwlink", std::bind(&WiFiTask::HandleWifiSetup, this));			  // Microsoft wifi portal.
	web_server_task_.AddHandler("/hotspot-detect.html", std::bind(&WiFiTask::HandleWifiSetup, this)); // Apple wifi portal.
}

void WiFiTask::run()
{
	while (true)
	{
		doubleResetDetector->loop();

		if (dnsServer)
			dnsServer->processNextRequest();

		// Turn off config mode after 10 minutes so that the device isn't in AP mode forever - a reboot will restart it
		if (_inConfigMode && (millis() - configModeStartTime > 1000 * 60 * 10))
			ConfigModeStop();

		updateDisplayStatus();

		delay(1000); // Check WiFi status every second
	}
}

bool WiFiTask::HandleCaptivePortal(String serverHostname)
{
	serverHostname.toLowerCase();
	if (!isIp(serverHostname) && serverHostname != _hostNameFQDN)
	{
		logger_.log("wifi: captive portal redirect to config");
		RedirectToConfig();
		return true;
	}
	return false;
}

void WiFiTask::ConfigModeStart()
{
	if (_inConfigMode)
		return;

	_inConfigMode = true;
	configModeStartTime = millis();

	WiFi.mode(WIFI_AP);
	WiFi.softAP(_apHostName.c_str(), AP_PASSWORD);
	logger_.logf("wifi: starting AP mode: %s", _apHostName.c_str());
	delay(100); // Pause for it to start

	// Setup DNS to redirect all domains to the AP
	dnsServer = new DNSServer();
	dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer->start(53, "*", WiFi.softAPIP());
}

void WiFiTask::ConfigModeStop(String ssid, String password)
{
	if (!_inConfigMode)
		return;

	logger_.log("wifi: stopping AP mode");
	_inConfigMode = false;

	if (dnsServer)
	{
		dnsServer->stop();
		dnsServer = nullptr;
	}

	WiFi.mode(WIFI_STA);
	if (ssid == "")
		WiFi.begin();
	else
		WiFi.begin(ssid.c_str(), password.c_str());
}

void WiFiTask::UpdateSSID(String ssid, String password)
{
	_attemptingNewSSID = true;
	if (_inConfigMode)
		ConfigModeStop(ssid, password);
	else
		WiFi.begin(ssid.c_str(), password.c_str());
}

void WiFiTask::RedirectToConfig()
{
	web_server_task_.Redirect(String("http://") + _hostNameFQDN + String("/network/config"));
}

void WiFiTask::HandleWifiSetup()
{
	/* Returning Success makes Apple happy and think there is a wifi connection - but for now I just want to load the config page on the captive portal */
	// server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
	// server.sendHeader("Pragma", "no-cache");
	// server.sendHeader("Expires", "-1");
	// server.send(200, "text/html", F("<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"));
	RedirectToConfig();
	return;
}

void WiFiTask::HandleConfigPage()
{
	web_server_task_.RespondWithFileOr404("/network/config.html");
}

void WiFiTask::UpdateNetworkSettings()
{
	// Called from the Wifi config form to set the new ssid and password
	String ssid = "";
	String pass = "";
	if (web_server_task_.GetRequestArg("ssid", ssid) && web_server_task_.GetRequestArg("pass", pass))
	{
		UpdateSSID(ssid, pass);
		web_server_task_.RespondWithFileOr404("/network/updating.html");
		return;
	}

	// File not found
	web_server_task_.RespondWith404();
}

void WiFiTask::GetNetworkList()
{
	String networks = "";
	int numberOfNetworks = WiFi.scanNetworks(false, false);

	if (numberOfNetworks == 0)
	{
		web_server_task_.RespondWithContent(204, "");
		return;
	}

	// Start with all networks
	int indices[numberOfNetworks];
	for (int i = 0; i < numberOfNetworks; i++)
		indices[i] = i;

	// Sort by signal strength
	for (int i = 0; i < numberOfNetworks; i++)
		for (int j = i + 1; j < numberOfNetworks; j++)
			if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i]))
				std::swap(indices[i], indices[j]);

	// Build response list, removing dups
	for (int i = 0; i < numberOfNetworks; i++)
	{
		// Skip this network if we've already seen it
		if (indices[i] == -1)
			continue;
		String ssid = WiFi.SSID(indices[i]);
		for (int j = i + 1; j < numberOfNetworks; j++)
		{
			// If a future network matches this name, skip it.
			if (indices[j] != -1 && ssid == WiFi.SSID(indices[j]))
				indices[j] = -1;
		}
		if (networks != "")
			networks += ",";
		networks += ssid;
	}

	web_server_task_.RespondWithContent(200, networks);
}

void WiFiTask::updateDisplayStatus()
{
	String ssid = "WiFi: " + (_inConfigMode ? ("AP: " + _apHostName) : WiFi.SSID());
	if (!ssid.equals(last_status_ssid))
	{
		last_status_ssid = ssid;
		logger_.log(ssid.c_str());
		display_task_.setMessage(1, ssid.c_str());
	}
}
