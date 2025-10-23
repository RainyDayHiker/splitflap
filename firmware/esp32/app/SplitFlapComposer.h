#pragma once

#include <PicoMQTT.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include "../core/logger.h"
#include "../core/splitflap_task.h"
#include "../core/task.h"

class SimpleWebServer; // Forward declaration

class SplitFlapComposer : public Task<SplitFlapComposer>
{
	friend class Task<SplitFlapComposer>; // Allow base Task to invoke protected run()

public:
	SplitFlapComposer(SplitflapTask &splitFlapTask, Logger &logger, const uint8_t task_core = 0);

	// Register web handlers related to splitflap operations
	void registerHandlers(SimpleWebServer &webServer);

protected:
	void run();

private:
	SplitflapTask &splitFlap;
	Logger &logger;

public:
	SplitFlapComposer();

	void Setup();

	// Public methods for external message forwarding (e.g., from MqttListener)
	// These allow the composer to work standalone or receive messages from another component
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
