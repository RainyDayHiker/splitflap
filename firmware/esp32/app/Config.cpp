#include <LittleFS.h>

#include "Config.h"

#define CONFIG_FILENAME "/properties/config.json"

bool fVerboseLog = true;

Config *configSingleton = nullptr;
Config *Config::GetInstance() { return configSingleton; }

Config::Config(SimpleWebServer &webServer, Logger &logger) : Task("Config", 8192, 1, 0),
															 webServer(webServer), logger(logger),
															 _dirty(false), _ready(false)
{
	configSingleton = this;

	// Defaults for properties
	_tz = LocalTime::LosAngeles;
	_quietTimeStartHour = 22;
	_quietTimeStartMinute = 0;
	_quietTimeEndHour = 7;
	_quietTimeEndMinute = 0;
	_autoStatusUpdatesEnabled = true;
	_mqttBroker = "mqtt.local";
	_mqttPort = 1883;
}

void Config::Setup()
{
	EnsureReady();

	webServer.AddHandler("/properties/set", HTTP_POST, std::bind(&Config::HandleSetProperties, this));
	// Get is handled by just letting the normal file serving handle delivering CONFIG_FILENAME
}

void Config::run()
{
	while (1)
	{
		SaveConfig();
		delay(10 * 1000);
	}
}

void Config::HandleSetProperties()
{
	if (ProcessSetProperties())
		webServer.RespondWithContent(200, "");
	else
		webServer.RespondWithContent(400, "No properties to set");
}

void Config::EnsureReady()
{
	if (_ready)
		return;

	// Load or create the file - this allows us to not have to sync defaults with the webserver
	if (LittleFS.exists(CONFIG_FILENAME))
	{
		LoadConfig();
	}
	else
	{
		_dirty = true;
		SaveConfig();
	}

	_ready = true;
}

void Config::SaveConfig()
{
	if (!_dirty)
		return;

	File file = LittleFS.open(CONFIG_FILENAME, "w", true);
	if (file)
	{
		JsonDocument doc;

		Save(&doc);

		serializeJson(doc, file);
		file.close();
		_dirty = false;
	}
	else
		logger.logf("config: failed to create file");
}

void Config::LoadConfig()
{
	JsonDocument doc;
	File file = LittleFS.open(CONFIG_FILENAME, "r");
	if (file)
	{
		DeserializationError error = deserializeJson(doc, file);
		if (error)
			logger.logf("config: deserializeJson failed: %s", error.f_str());
		file.close();
	}
	else
		logger.logf("config: failed to open the file");

	Load(&doc);
}

bool Config::ProcessSetProperties()
{
	bool processedSomething = false;

	String argValue = "";
	if (webServer.GetRequestArg("timezone", argValue))
		processedSomething |= SetTimeZone(argValue.toInt());

	// Handle quiet time start settings
	String quietTimeStartHour, quietTimeStartMinute;
	if (webServer.GetRequestArg("quietTimeStartHour", quietTimeStartHour) &&
		webServer.GetRequestArg("quietTimeStartMinute", quietTimeStartMinute))
	{
		processedSomething |= SetQuietTimeStart(quietTimeStartHour.toInt(), quietTimeStartMinute.toInt());
	}

	// Handle quiet time end settings
	String quietTimeEndHour, quietTimeEndMinute;
	if (webServer.GetRequestArg("quietTimeEndHour", quietTimeEndHour) &&
		webServer.GetRequestArg("quietTimeEndMinute", quietTimeEndMinute))
	{
		processedSomething |= SetQuietTimeEnd(quietTimeEndHour.toInt(), quietTimeEndMinute.toInt());
	}

	// Handle auto status updates toggle (expecting "autoStatusUpdates=0" or "1")
	String autoStatusUpdates;
	if (webServer.GetRequestArg("autoStatusUpdates", autoStatusUpdates))
	{
		int val = autoStatusUpdates.toInt();
		processedSomething |= SetAutoStatusUpdatesEnabled(val != 0);
	}

	// Handle MQTT broker setting
	String mqttBroker;
	if (webServer.GetRequestArg("mqttBroker", mqttBroker))
	{
		processedSomething |= SetMqttBroker(mqttBroker);
	}

	// Handle MQTT port setting
	String mqttPort;
	if (webServer.GetRequestArg("mqttPort", mqttPort))
	{
		processedSomething |= SetMqttPort(mqttPort.toInt());
	}

	return processedSomething;
}

void Config::Load(JsonDocument *doc)
{
	JsonDocument &docRef = *doc;
	if (docRef["TimeZone"].is<int>())
		SetTimeZone((int)docRef["TimeZone"]);

	// Load quiet time start time
	if (docRef["QuietTimeStartHour"].is<int>() && docRef["QuietTimeStartMinute"].is<int>())
		SetQuietTimeStart(docRef["QuietTimeStartHour"], docRef["QuietTimeStartMinute"]);

	// Load quiet time end time
	if (docRef["QuietTimeEndHour"].is<int>() && docRef["QuietTimeEndMinute"].is<int>())
		SetQuietTimeEnd(docRef["QuietTimeEndHour"], docRef["QuietTimeEndMinute"]);

	// Load auto status updates
	if (docRef["AutoStatusUpdates"].is<bool>())
		SetAutoStatusUpdatesEnabled((bool)docRef["AutoStatusUpdates"]);

	// Load MQTT broker
	if (docRef["MqttBroker"].is<const char *>())
		SetMqttBroker(docRef["MqttBroker"].as<const char *>());

	// Load MQTT port
	if (docRef["MqttPort"].is<int>())
		SetMqttPort(docRef["MqttPort"]);
}

void Config::Save(JsonDocument *doc)
{
	JsonDocument &docRef = *doc;
	docRef["TimeZone"] = _tz;

	// Save quiet time start time
	docRef["QuietTimeStartHour"] = _quietTimeStartHour;
	docRef["QuietTimeStartMinute"] = _quietTimeStartMinute;

	// Save quiet time end time
	docRef["QuietTimeEndHour"] = _quietTimeEndHour;
	docRef["QuietTimeEndMinute"] = _quietTimeEndMinute;

	// Save auto status updates setting
	docRef["AutoStatusUpdates"] = _autoStatusUpdatesEnabled;

	// Save MQTT settings
	docRef["MqttBroker"] = _mqttBroker;
	docRef["MqttPort"] = _mqttPort;
}

bool Config::SetTimeZone(LocalTime::TimeZone tzNew)
{
	if (_tz == tzNew)
		return false;

	if (fVerboseLog)
		logger.logf("Config: Setting timezone to %d", tzNew);

	_tz = tzNew;
	_dirty = true;
	LocalTime::UpdateTimeZone(_tz);
	return true;
}

bool Config::SetTimeZone(int tzNew)
{
	switch (tzNew)
	{
	default:
	case LocalTime::Universal:
		return SetTimeZone(LocalTime::Universal);
		break;
	case LocalTime::NewYork:
		return SetTimeZone(LocalTime::NewYork);
		break;
	case LocalTime::Chicago:
		return SetTimeZone(LocalTime::Chicago);
		break;
	case LocalTime::Denver:
		return SetTimeZone(LocalTime::Denver);
		break;
	case LocalTime::LosAngeles:
		return SetTimeZone(LocalTime::LosAngeles);
		break;
	}
	return false;
}

bool Config::SetQuietTimeStart(int hour, int minute)
{
	// Validate input
	if (!IsValidTime(hour, minute))
		return false;

	if (_quietTimeStartHour == hour && _quietTimeStartMinute == minute)
		return false;

	if (fVerboseLog)
		logger.logf("Config: Setting quiet time start to %02d:%02d", hour, minute);

	_quietTimeStartHour = hour;
	_quietTimeStartMinute = minute;
	_dirty = true;
	return true;
}

bool Config::SetQuietTimeEnd(int hour, int minute)
{
	// Validate input
	if (!IsValidTime(hour, minute))
		return false;

	if (_quietTimeEndHour == hour && _quietTimeEndMinute == minute)
		return false;

	if (fVerboseLog)
		logger.logf("Config: Setting quiet time end to %02d:%02d", hour, minute);

	_quietTimeEndHour = hour;
	_quietTimeEndMinute = minute;
	_dirty = true;
	return true;
}

bool Config::IsTimeInQuietPeriod(const tm *time)
{
	if (!time)
		return false;

	int startMinutes = _quietTimeStartHour * 60 + _quietTimeStartMinute;
	int endMinutes = _quietTimeEndHour * 60 + _quietTimeEndMinute;
	int currentMinutes = time->tm_hour * 60 + time->tm_min;

	// If start and end are identical, treat as no quiet period.
	if (startMinutes == endMinutes)
		return false;

	if (startMinutes < endMinutes)
	{
		// Quiet period does not cross midnight: [start, end)
		return currentMinutes >= startMinutes && currentMinutes < endMinutes;
	}
	else
	{
		// Quiet period crosses midnight: from start -> 24:00 and 00:00 -> end
		return (currentMinutes >= startMinutes) || (currentMinutes < endMinutes);
	}
}

bool Config::IsValidTime(int hour, int minute)
{
	// Check for valid hour (0-23) and minute (0-59)
	return (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59);
}

bool Config::SetAutoStatusUpdatesEnabled(bool enabled)
{
	if (_autoStatusUpdatesEnabled == enabled)
		return false;
	if (fVerboseLog)
		logger.logf("Config: Setting auto status updates to %s", enabled ? "enabled" : "disabled");
	_autoStatusUpdatesEnabled = enabled;
	_dirty = true;
	return true;
}

bool Config::SetMqttBroker(const String &broker)
{
	if (_mqttBroker == broker)
		return false;

	if (fVerboseLog)
		logger.logf("Config: Setting MQTT broker to %s", broker.c_str());

	_mqttBroker = broker;
	_dirty = true;
	return true;
}

bool Config::SetMqttPort(int port)
{
	if (_mqttPort == port)
		return false;

	// Validate port range
	if (port < 1 || port > 65535)
	{
		if (fVerboseLog)
			logger.logf("Config: Invalid MQTT port %d", port);
		return false;
	}

	if (fVerboseLog)
		logger.logf("Config: Setting MQTT port to %d", port);

	_mqttPort = port;
	_dirty = true;
	return true;
}
