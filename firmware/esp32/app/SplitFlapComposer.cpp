#include "SplitFlapComposer.h"
#include <ArduinoJson.h>
#include "LocalTime.h"
#include "Config.h"
#include "SplitFlap.h"

SplitFlapComposer::SplitFlapComposer(SplitFlap &splitFlap, Logger &logger, const uint8_t task_core)
	: Task("SplitFlapComposer", 8192, 1, task_core),
	  splitFlap(splitFlap),
	  logger(logger),
	  _latestHandicap(""),
	  _latestTemperature(""),
	  _hasHandicap(false),
	  _hasTemperature(false),
	  _temporaryMessage(""),
	  _temporaryMessageExpiry(0),
	  _hasTemporaryMessage(false),
	  _pendingDisplayMessage(""),
	  _hasPendingDisplayMessage(false),
	  _lastDisplayUpdateMillis(0),
	  _lastPublishedMinute(-1)
{
	logger.logf("MQTT Broker setup: %s:%d", Config::GetInstance()->GetMqttBroker().c_str(), Config::GetInstance()->GetMqttPort());
	if (Config::GetInstance()->GetMqttEnabled())
	{
		_mqttClient.host = Config::GetInstance()->GetMqttBroker();
		_mqttClient.port = Config::GetInstance()->GetMqttPort();

		// Subscribe to golf handicap messages (matches house/golf/anything)
		_mqttClient.subscribe("house/golf/#", [this](const char *topic, const char *payload)
							  { OnHandicapMessage(topic, payload); });

		// Subscribe to weather messages (exact match for the published topic)
		_mqttClient.subscribe("house/weather/", [this](const char *topic, const char *payload)
							  { OnWeatherMessage(topic, payload); });

		// Subscribe to Split messages (exact match for the published topic)
		_mqttClient.subscribe("house/splitflap/message", [this](const char *topic, const char *payload)
							  { OnCustomMessage(topic, payload); });

		// Subscribe to display messages to update the split-flap directly
		_mqttClient.subscribe("house/splitflap/display", [this](const char *topic, const char *payload)
							  { OnDisplayMessage(topic, payload); });
	}
	logger.log("SplitFlapComposer setup complete");

	// Trigger initial display update to show connection status
	PublishComposedMessage();
}

void SplitFlapComposer::run()
{
	while (1)
	{
		// Check if MQTT is enabled
		if (Config::GetInstance()->GetMqttEnabled())
		{
			// Check for MQTT configuration changes
			if (_mqttClient.host != Config::GetInstance()->GetMqttBroker() || _mqttClient.port != Config::GetInstance()->GetMqttPort())
			{
				logger.log("MQTT configuration changed, reconnecting...");

				_mqttClient.host = Config::GetInstance()->GetMqttBroker();
				_mqttClient.port = Config::GetInstance()->GetMqttPort();
				_mqttClient.disconnect();
			}

			_mqttClient.loop(); // Process MQTT messages

			// Yield after processing MQTT to prevent watchdog timeout
			taskYIELD();
		}
		else
		{
			// If MQTT is disabled, disconnect if currently connected
			if (_mqttClient.connected())
			{
				logger.log("MQTT disabled, disconnecting...");
				_mqttClient.disconnect();
			}
			// Yield when MQTT is disabled to prevent tight loop
			taskYIELD();
		}

		// Check if temporary message has expired
		if (_hasTemporaryMessage)
		{
			time_t now = LocalTime::GetCurrentTime(nullptr);
			if (now >= _temporaryMessageExpiry)
			{
				logger.log("Temporary message expired, reverting to normal display");
				_hasTemporaryMessage = false;
				_temporaryMessage = "";

				// Republish the normal composed message
				PublishComposedMessage();
			}
		} // Check if time has changed and update display if needed
		CheckAndUpdateTime();

		// Process any pending display updates with throttling
		ProcessPendingDisplayUpdate();

		// Shorter delay to yield more frequently and prevent watchdog timeout
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

void SplitFlapComposer::OnHandicapMessage(const char *topic, const char *payload)
{
	logger.log("SplitFlapComposer.OnHandicapMessage called");

	// Parse the JSON payload to extract the handicap index
	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, payload);

	if (error)
	{
		logger.logf("Failed to parse handicap JSON: %s", error.c_str());
		return;
	}

	// Extract the handicap index value
	if (doc["handicapIndex"].is<float>())
	{
		float handicapValue = doc["handicapIndex"];
		_latestHandicap = String(handicapValue, 1); // Format with 1 decimal place
		_hasHandicap = true;

		logger.logf("Updated handicap value: %s", _latestHandicap.c_str());

		// Publish the composed message
		PublishComposedMessage();
	}
	else
	{
		logger.log("Handicap message did not contain 'handicapIndex' field");
	}
}

void SplitFlapComposer::OnWeatherMessage(const char *topic, const char *payload)
{
	logger.log("SplitFlapComposer.OnWeatherMessage called");

	// Parse the JSON payload to extract the temperature
	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, payload);

	if (error)
	{
		logger.logf("Failed to parse weather JSON: %s", error.c_str());
		return;
	}

	// Extract the current temperature value
	if (doc["temperature"].is<float>())
	{
		float tempValue = doc["temperature"];
		_latestTemperature = String((int)tempValue); // Format as integer
		_hasTemperature = true;

		logger.logf("Updated temperature value: %s", _latestTemperature.c_str());

		// Publish the composed message
		PublishComposedMessage();
	}
	else
	{
		logger.log("Weather message did not contain 'temperature' field");
	}
}

void SplitFlapComposer::PublishComposedMessage()
{
	// Skip if a temporary message is active
	if (_hasTemporaryMessage)
		return;

	// Get current time in hh:mm format if time sync has happened
	String timeStr = "      "; // 6 spaces as default
	if (LocalTime::HasTimeSyncHappened())
	{
		struct tm timeinfo;
		time_t now = LocalTime::GetCurrentTime(&timeinfo);
		char timeBuf[6]; // "hh:mm\0"
		snprintf(timeBuf, sizeof(timeBuf), "%2d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
		timeStr = String(timeBuf);

		// Update last published minute
		_lastPublishedMinute = timeinfo.tm_min;
	}

	// Compose the message: "hh:mm handicap temperature" (time is always 6 chars, blank if no sync)
	String composedMessage = timeStr + _latestHandicap + " " + _latestTemperature;

	if (!_mqttClient.connected())
	{
		logger.log("MQTT not connected, setting display message directly");

		// Ensure message is at least 6 characters long, padding with spaces if needed
		while (composedMessage.length() < 6)
			composedMessage += " ";

		// Set 6th character to 'b' (0-indexed position 5)
		composedMessage.setCharAt(5, 'b');

		// Set directly to pending display message
		SetPendingDisplayMessage(composedMessage);
		return;
	}

	String topic = "house/splitflap/display";

	bool published = _mqttClient.publish(topic, composedMessage);

	if (published)
	{
		logger.logf("Published split-flap message: '%s' to topic: %s",
					composedMessage.c_str(), topic.c_str());
	}
	else
	{
		logger.log("Failed to publish split-flap message to MQTT");
	}
}

void SplitFlapComposer::OnCustomMessage(const char *topic, const char *payload)
{
	logger.logf("SplitFlapComposer.OnCustomMessage called for topic: %s", topic);

	// Parse the JSON payload to extract the message
	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, payload);

	if (error)
	{
		logger.logf("Failed to parse custom message JSON: %s", error.c_str());
		return;
	}

	// Extract the message value
	if (doc["message"].is<String>())
	{
		_temporaryMessage = doc["message"].as<String>();
		_hasTemporaryMessage = true;

		// Set expiry time to 5 minutes from now
		time_t now = LocalTime::GetCurrentTime(nullptr);
		_temporaryMessageExpiry = now + TEMPORARY_MESSAGE_DURATION;

		logger.logf("Received temporary message: '%s', will expire in %d seconds",
					_temporaryMessage.c_str(), TEMPORARY_MESSAGE_DURATION);

		// Immediately publish the temporary message
		PublishTemporaryMessage();
	}
	else
	{
		logger.log("Custom message did not contain 'message' field");
	}
}

void SplitFlapComposer::PublishTemporaryMessage()
{
	if (!_mqttClient.connected())
	{
		logger.log("MQTT not connected, cannot publish temporary message");
		return;
	}

	String topic = "house/splitflap/display";

	bool published = _mqttClient.publish(topic, _temporaryMessage);

	if (published)
	{
		logger.logf("Published temporary split-flap message: '%s' to topic: %s",
					_temporaryMessage.c_str(), topic.c_str());
	}
	else
	{
		logger.log("Failed to publish temporary split-flap message to MQTT");
	}
}

void SplitFlapComposer::OnDisplayMessage(const char *topic, const char *payload)
{
	logger.logf("SplitFlapComposer.OnDisplayMessage called with payload: %s", payload);

	// Queue the message instead of blocking - it will be processed in the main loop
	SetPendingDisplayMessage(String(payload));
}

void SplitFlapComposer::ProcessPendingDisplayUpdate()
{
	if (!_hasPendingDisplayMessage)
	{
		return;
	}

	// Throttle updates to prevent queue overflow
	unsigned long now = millis();
	if (now - _lastDisplayUpdateMillis < MIN_DISPLAY_UPDATE_INTERVAL_MS)
	{
		return; // Too soon, skip this update
	}

	// Process the update
	splitFlap.SetDisplayMessage(_pendingDisplayMessage);
	_hasPendingDisplayMessage = false;
	_lastDisplayUpdateMillis = now;
	logger.logf("Processed pending display update: %s", _pendingDisplayMessage.c_str());
}

void SplitFlapComposer::CheckAndUpdateTime()
{
	// Skip if time sync hasn't happened yet
	if (!LocalTime::HasTimeSyncHappened())
		return;

	// Get current time
	struct tm timeinfo;
	LocalTime::GetCurrentTime(&timeinfo);

	// Check if the minute has changed
	if (_lastPublishedMinute != timeinfo.tm_min)
	{
		logger.logf("Time changed from minute %d to %d, updating display",
					_lastPublishedMinute, timeinfo.tm_min);
		PublishComposedMessage();
	}
}

void SplitFlapComposer::SetPendingDisplayMessage(const String &message)
{
	_pendingDisplayMessage = message;
	_hasPendingDisplayMessage = true;
}
