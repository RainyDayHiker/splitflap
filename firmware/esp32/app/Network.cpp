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

#include "Network.h"
#include "../splitflap/secrets.h"
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
			return false;
	}
	return true;
}

DNSServer *dnsServer;
DoubleResetDetector *doubleResetDetector;
uint32_t configModeStartTime = 0;

Network::Network(DisplayTask &display_task, SimpleWebServer &webServer, Logger &logger, const uint8_t task_core)
	: Task("Network", 4096, 1, task_core),
	  displayTask(display_task),
	  webServer(webServer),
	  logger(logger),
	  lastStatusSSID("")
{
	hostNameFQDN = DEVICE_INSTANCE_NAME;
	hostNameFQDN += ".local";
	hostNameFQDN.toLowerCase();

	apHostName = DEVICE_INSTANCE_NAME;
	apHostName += String((long)ESP.getEfuseMac(), HEX);
}

void Network::Setup()
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
		logger.log("wifi: Double reset detected");
		ConfigModeStart();
	}
	else
	{
		// check for cached WiFi credentials
		wifi_config_t cachedConfig;
		esp_err_t configResult = esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &cachedConfig);
		if (configResult == ESP_OK && strlen((const char *)cachedConfig.ap.ssid) > 0)
		{
			logger.logf("wifi: cached SSID: %s found, connecting", cachedConfig.ap.ssid);
			WiFi.begin();
		}
		else if (wifiSSID != "" && wifiPassword != "")
		{
			logger.logf("wifi: Attempting to use build provided ssid (%s) and password", wifiSSID.c_str());
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
	webServer.AddHandler("/network/config", std::bind(&Network::HandleConfigPage, this));
	webServer.AddHandler("/network/network_update", std::bind(&Network::UpdateNetworkSettings, this));
	webServer.AddHandler("/network/networks", std::bind(&Network::GetNetworkList, this));

	// Handle the various wifi config landing pages
	webServer.AddHandler("/generate_204", std::bind(&Network::HandleWifiSetup, this));		  // Android wifi portal.
	webServer.AddHandler("/fwlink", std::bind(&Network::HandleWifiSetup, this));			  // Microsoft wifi portal.
	webServer.AddHandler("/hotspot-detect.html", std::bind(&Network::HandleWifiSetup, this)); // Apple wifi portal.
}

void Network::run()
{
	while (true)
	{
		doubleResetDetector->loop();

		if (dnsServer)
			dnsServer->processNextRequest();

		// Turn off config mode after 10 minutes so that the device isn't in AP mode forever - a reboot will restart it
		if (inConfigMode && (millis() - configModeStartTime > 1000 * 60 * 10))
			ConfigModeStop();

		updateDisplayStatus();

		delay(1000); // Check WiFi status every second
	}
}

bool Network::HandleCaptivePortal(String serverHostname)
{
	serverHostname.toLowerCase();
	if (!isIp(serverHostname) && serverHostname != hostNameFQDN)
	{
		logger.log("wifi: captive portal redirect to config");
		RedirectToConfig();
		return true;
	}
	return false;
}

void Network::ConfigModeStart()
{
	if (inConfigMode)
		return;

	inConfigMode = true;
	configModeStartTime = millis();

	WiFi.mode(WIFI_AP);
	WiFi.softAP(apHostName.c_str(), AP_PASSWORD);
	logger.logf("wifi: starting AP mode: %s", apHostName.c_str());
	delay(100); // Pause for it to start

	// Setup DNS to redirect all domains to the AP
	dnsServer = new DNSServer();
	dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer->start(53, "*", WiFi.softAPIP());
}

void Network::ConfigModeStop(String ssid, String password)
{
	if (!inConfigMode)
		return;

	logger.log("wifi: stopping AP mode");
	inConfigMode = false;

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

void Network::UpdateSSID(String ssid, String password)
{
	attemptingNewSSID = true;
	if (inConfigMode)
		ConfigModeStop(ssid, password);
	else
		WiFi.begin(ssid.c_str(), password.c_str());
}

void Network::RedirectToConfig()
{
	webServer.Redirect(String("http://") + hostNameFQDN + String("/network/config"));
}

void Network::HandleWifiSetup()
{
	/* Returning Success makes Apple happy and think there is a wifi connection - but for now I just want to load the config page on the captive portal */
	// server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
	// server.sendHeader("Pragma", "no-cache");
	// server.sendHeader("Expires", "-1");
	// server.send(200, "text/html", F("<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"));
	RedirectToConfig();
	return;
}

void Network::HandleConfigPage()
{
	webServer.RespondWithFileOr404("/network/config.html");
}

void Network::UpdateNetworkSettings()
{
	// Called from the Wifi config form to set the new ssid and password
	String ssid = "";
	String pass = "";
	if (webServer.GetRequestArg("ssid", ssid) && webServer.GetRequestArg("pass", pass))
	{
		UpdateSSID(ssid, pass);
		webServer.RespondWithFileOr404("/network/updating.html");
		return;
	}

	// File not found
	webServer.RespondWith404();
}

void Network::GetNetworkList()
{
	String networks = "";
	int numberOfNetworks = WiFi.scanNetworks(false, false);

	if (numberOfNetworks == 0)
	{
		webServer.RespondWithContent(204, "");
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

	webServer.RespondWithContent(200, networks);
}

void Network::updateDisplayStatus()
{
	String ssid = "WiFi: " + (inConfigMode ? ("AP: " + apHostName) : WiFi.SSID());
	if (!ssid.equals(lastStatusSSID))
	{
		lastStatusSSID = ssid;
		logger.log(ssid.c_str());
		displayTask.setMessage(1, ssid.c_str());
	}
}
