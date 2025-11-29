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
	  _latestTemperature(0),
	  _latestConditionCode(1000),
	  _latestWindSpeed(0.0),
	  _hasHandicap(false),
	  _hasTemperature(false),
	  _latestStockPrice(0.0),
	  _latestStockIsUp(false),
	  _hasStock(false),
	  _timeString("      "),
	  _stockString("      "),
	  _handicapString("      "),
	  _weatherString("      "),
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

		// Subscribe to stock messages (exact match for MSFT)
		_mqttClient.subscribe("house/stock/MSFT", [this](const char *topic, const char *payload)
							  { OnStockMessage(topic, payload); });
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

		// Update handicap string
		_handicapString = "H:" + _latestHandicap;
		while (_handicapString.length() < 6)
			_handicapString += " ";
		if (_handicapString.length() > 6)
			_handicapString = _handicapString.substring(0, 6);

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

		// Clamp temperature to range -99 to 999
		_latestTemperature = constrain((int)tempValue, -99, 999);
		_hasTemperature = true;

		// Extract condition code if available, default to 1000 (sunny/clear)
		_latestConditionCode = doc["conditionCode"] | 1000;

		// Extract wind speed if available, default to 0.0
		_latestWindSpeed = doc["windSpeed"] | 0.0;

		logger.logf("Updated weather: temp=%d, condition=%d, wind=%.1f", _latestTemperature, _latestConditionCode, _latestWindSpeed);

		// Update weather string: 2 spaces + temp (3 chars, right-aligned) + condition indicator
		char tempBuf[4]; // 3 chars + null terminator
		snprintf(tempBuf, sizeof(tempBuf), "%3d", _latestTemperature);
		char conditionChar = MapConditionCodeToChar(_latestConditionCode, _latestWindSpeed);
		_weatherString = "  " + String(tempBuf) + conditionChar;

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

	// Compose the message from each 6-character section: time, stock, handicap, weather
	String composedMessage = _timeString + _handicapString + _stockString + _weatherString;

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

void SplitFlapComposer::OnStockMessage(const char *topic, const char *payload)
{
	logger.logf("SplitFlapComposer.OnStockMessage called for topic: %s", topic);

	// Parse the JSON payload to extract price and change
	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, payload);

	if (error)
	{
		logger.logf("Failed to parse stock JSON: %s", error.c_str());
		return;
	}

	// Extract the price and change values
	if (doc["price"].is<float>() && doc["change"].is<float>())
	{
		float price = doc["price"];
		float change = doc["change"];

		// Store price and direction, clamping price to fit in 5 characters (max 999.9, min -99.9)
		_latestStockPrice = constrain(price, -99.9f, 999.9f);
		_latestStockIsUp = (change >= 0);
		_hasStock = true;

		logger.logf("Updated stock: price=%.1f, isUp=%s",
					_latestStockPrice,
					_latestStockIsUp ? "yes" : "no");

		// Update stock string: 2 spaces + price (5 chars, right-aligned) + indicator (a=up, b=down)
		char priceBuf[6]; // 5 chars + null terminator
		snprintf(priceBuf, sizeof(priceBuf), "%5.1f", _latestStockPrice);
		_stockString = String(priceBuf) + (_latestStockIsUp ? "a" : "b");

		// Publish the composed message
		PublishComposedMessage();
	}
	else
	{
		logger.log("Stock message did not contain 'price' and 'change' fields");
	}
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

		// Update the time string (6 characters: "hh:mm " in 12-hour format)
		struct tm timeinfo;
		LocalTime::GetCurrentTime(&timeinfo);

		// Update last published minute
		_lastPublishedMinute = timeinfo.tm_min;

		// Convert to 12-hour format
		int hour12 = timeinfo.tm_hour % 12;
		if (hour12 == 0)
			hour12 = 12; // 0 and 12 should display as 12

		char timeBuf[7]; // "hh:mm \0"
		snprintf(timeBuf, sizeof(timeBuf), "%2d:%02d ", hour12, timeinfo.tm_min);
		_timeString = String(timeBuf);

		// Publish the composed message
		PublishComposedMessage();
	}
}

void SplitFlapComposer::SetPendingDisplayMessage(const String &message)
{
	_pendingDisplayMessage = message;
	_hasPendingDisplayMessage = true;
}

char SplitFlapComposer::MapConditionCodeToChar(int conditionCode, float windSpeed)
{
	// Map condition codes to display characters:
	// a = sun, b = cloud, c = rain, d = snow, e = wind, f = partly cloudy

	char baseChar;

	switch (conditionCode)
	{
	// Sunny/Clear
	case 1000:
		baseChar = 'a';
		break;

	// Partly cloudy
	case 1003:
		baseChar = 'f';
		break;

	// Cloudy/Overcast/Mist/Fog
	case 1006:
	case 1009:
	case 1030:
	case 1135:
	case 1147:
		baseChar = 'b';
		break;

	// Rain (all drizzle and rain conditions)
	case 1063:
	case 1072:
	case 1150:
	case 1153:
	case 1168:
	case 1171:
	case 1180:
	case 1183:
	case 1186:
	case 1189:
	case 1192:
	case 1195:
	case 1198:
	case 1201:
	case 1240:
	case 1243:
	case 1246:
	case 1273:
	case 1276:
		baseChar = 'c';
		break;

	// Snow (all snow, sleet, and ice conditions)
	case 1066:
	case 1069:
	case 1114:
	case 1117:
	case 1204:
	case 1207:
	case 1210:
	case 1213:
	case 1216:
	case 1219:
	case 1222:
	case 1225:
	case 1237:
	case 1249:
	case 1252:
	case 1255:
	case 1258:
	case 1261:
	case 1264:
	case 1279:
	case 1282:
		baseChar = 'd';
		break;

	// Thunderstorms (wind-related)
	case 1087:
		baseChar = 'e';
		break;

	// Default to cloud for unknown codes
	default:
		baseChar = 'b';
		break;
	}

	// Override with wind icon if wind speed > 15 and condition is sun, cloud, or partly cloudy
	if (windSpeed > 15.0 && (baseChar == 'a' || baseChar == 'b' || baseChar == 'f'))
	{
		return 'e';
	}

	return baseChar;
}
