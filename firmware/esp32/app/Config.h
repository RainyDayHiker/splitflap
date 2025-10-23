#pragma once
#include <ArduinoJson.h>
#include "../core/task.h"

#include "SimpleWebServer.h"
#include "LocalTime.h"

#define CONFIG_JSON_SIZE 800

class Config : public Task<Config>
{
	friend class Task<Config>; // Allow base Task to invoke protected run()

public:
	Config(SimpleWebServer &webServer, Logger &logger);

	static Config *GetInstance();

	void Setup();

	void HandleSetProperties();

protected:
	void run();

private:
	void EnsureReady();
	void SaveConfig();
	void LoadConfig();

	bool _dirty;
	bool _ready;

	SimpleWebServer &webServer;
	Logger &logger;

	bool ProcessSetProperties();
	void Load(JsonDocument *doc);
	void Save(JsonDocument *doc);
	bool IsValidTime(int hour, int minute);

	LocalTime::TimeZone _tz;
	int _quietTimeStartHour;
	int _quietTimeStartMinute;
	int _quietTimeEndHour;
	int _quietTimeEndMinute;
	bool _autoStatusUpdatesEnabled;
	String _mqttBroker;
	int _mqttPort;

public:
	LocalTime::TimeZone GetTimeZone() { return _tz; }
	bool SetTimeZone(LocalTime::TimeZone tzNew);
	bool SetTimeZone(int tzNew);

	int GetQuietTimeStartHour() { return _quietTimeStartHour; }
	int GetQuietTimeStartMinute() { return _quietTimeStartMinute; }
	bool SetQuietTimeStart(int hour, int minute);

	int GetQuietTimeEndHour() { return _quietTimeEndHour; }
	int GetQuietTimeEndMinute() { return _quietTimeEndMinute; }
	bool SetQuietTimeEnd(int hour, int minute);

	bool IsTimeInQuietPeriod(const struct tm *time);

	// Auto status updates
	bool GetAutoStatusUpdatesEnabled() const { return _autoStatusUpdatesEnabled; }
	bool SetAutoStatusUpdatesEnabled(bool enabled);

	// MQTT settings
	String GetMqttBroker() const { return _mqttBroker; }
	bool SetMqttBroker(const String &broker);

	int GetMqttPort() const { return _mqttPort; }
	bool SetMqttPort(int port);
};
