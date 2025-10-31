#pragma once

#include <PicoMQTT.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include "../core/logger.h"
#include "../core/task.h"

class SimpleWebServer; // Forward declaration
class SplitFlap;

class SplitFlapComposer : public Task<SplitFlapComposer>
{
	friend class Task<SplitFlapComposer>; // Allow base Task to invoke protected run()

public:
	SplitFlapComposer(SplitFlap &splitFlap, Logger &logger, const uint8_t task_core = 0);

protected:
	void run();

private:
	SplitFlap &splitFlap;
	Logger &logger;

	void OnHandicapMessage(const char *topic, const char *payload);
	void OnWeatherMessage(const char *topic, const char *payload);
	void OnCustomMessage(const char *topic, const char *payload);

private:
	PicoMQTT::Client _mqttClient;

	// Store the latest values
	String _latestHandicap;
	String _latestTemperature;
	bool _hasHandicap;
	bool _hasTemperature;

	// Temporary message state
	String _temporaryMessage;
	time_t _temporaryMessageExpiry;
	bool _hasTemporaryMessage;
	static const unsigned long TEMPORARY_MESSAGE_DURATION = 300; // 5 minutes in seconds

	// Publish the composed message
	void PublishComposedMessage();
	void PublishTemporaryMessage();
};
