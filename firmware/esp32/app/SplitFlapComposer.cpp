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
	  _hasTemporaryMessage(false)
{
	logger.logf("MQTT Broker setup: %s:%d", Config::GetInstance()->GetMqttBroker().c_str(), Config::GetInstance()->GetMqttPort());
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

	logger.log("SplitFlapComposer setup complete");
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
		}
		else
		{
			// If MQTT is disabled, disconnect if currently connected
			if (_mqttClient.connected())
			{
				logger.log("MQTT disabled, disconnecting...");
				_mqttClient.disconnect();
			}
		} // Check if temporary message has expired
		if (_hasTemporaryMessage)
		{
			time_t now = LocalTime::GetCurrentTime(nullptr);
			if (now >= _temporaryMessageExpiry)
			{
				logger.log("Temporary message expired, reverting to normal display");
				_hasTemporaryMessage = false;
				_temporaryMessage = "";

				// Republish the normal composed message
				if (_hasHandicap && _hasTemperature)
				{
					PublishComposedMessage();
				}
			}
		}

		// Test section.  Wait 5 seconds after starting then post a test message
		static bool testMessageSent = false;
		if (!testMessageSent)
		{
			static unsigned long startTime = millis();
			if (millis() - startTime > 5000)
			{
				if (Config::GetInstance()->GetAutoStatusUpdatesEnabled())
					splitFlap.SetDisplayMessage("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
				testMessageSent = true;
			}
		}

		delay(200);
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

		// Publish the composed message if we have both values (and no temporary message active)
		if (_hasHandicap && _hasTemperature)
		{
			if (!_hasTemporaryMessage)
				PublishComposedMessage();
		}
		else
		{
			logger.logf("Waiting for temperature before publishing (hasTemp=%s)", _hasTemperature ? "yes" : "no");
		}
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

		// Publish the composed message if we have both values (and no temporary message active)
		if (_hasHandicap && _hasTemperature)
		{
			if (!_hasTemporaryMessage)
				PublishComposedMessage();
		}
		else
		{
			logger.logf("Waiting for handicap before publishing (hasHandicap=%s)", _hasHandicap ? "yes" : "no");
		}
	}
	else
	{
		logger.log("Weather message did not contain 'temperature' field");
	}
}

void SplitFlapComposer::PublishComposedMessage()
{
	if (!_mqttClient.connected())
	{
		logger.log("MQTT not connected, cannot publish split-flap message");
		return;
	}

	// Compose the message: "handicap temperature"
	String composedMessage = _latestHandicap + " " + _latestTemperature;
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
