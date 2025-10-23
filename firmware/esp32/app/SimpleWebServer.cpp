#include "SimpleWebServer.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <FS.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ElegantOTA.h>

namespace mime
{
	enum type
	{
		html,
		htm,
		txt,
		css,
		js,
		json,
		png,
		gif,
		jpg,
		jpeg,
		ico,
		svg,
		ttf,
		otf,
		woff,
		woff2,
		eot,
		sfnt,
		xml,
		pdf,
		zip,
		appcache,
		gz,
		none,
		maxType
	};

	struct Entry
	{
		const char *endsWith;
		const char *mimeType;
	};

	const Entry mimeTable[maxType] = {
		{".html", "text/html; charset=utf-8"},
		{".htm", "text/html; charset=utf-8"},
		{".txt", "text/plain; charset=utf-8"},
		{".css", "text/css; charset=utf-8"},
		{".js", "text/javascript; charset=utf-8"},
		{".json", "application/json; charset=utf-8"},
		{".png", "image/png"},
		{".gif", "image/gif"},
		{".jpg", "image/jpeg"},
		{".jpeg", "image/jpeg"},
		{".ico", "image/x-icon"},
		{".svg", "image/svg+xml"},
		{".ttf", "font/ttf"},
		{".otf", "font/otf"},
		{".woff", "font/woff"},
		{".woff2", "font/woff2"},
		{".eot", "font/eot"},
		{".sfnt", "font/sfnt"},
		{".xml", "application/xml"},
		{".pdf", "application/pdf"},
		{".zip", "application/zip"},
		{".appcache", "text/cache-manifest; charset=utf-8"},
		{".gz", "application/gzip"},
		{".none", "application/octet-stream"}};

	String getContentType(const String &path)
	{
		for (size_t i = 0; i < maxType; i++)
			if (path.endsWith(FPSTR(mimeTable[i].endsWith)))
				return String(FPSTR(mimeTable[i].mimeType));

		// Fall-through and just return default type
		return String(FPSTR(mimeTable[none].mimeType));
	}
}

SimpleWebServer::SimpleWebServer(Logger &logger, const uint8_t task_core) : Task("WebServer", 4096, 1, task_core),
																			logger(logger),
																			server(nullptr),
																			running(false)
{
	server = new WebServer(SERVER_PORT);
}

SimpleWebServer::~SimpleWebServer()
{
	Stop();
}

bool SimpleWebServer::Start(std::function<bool(String)> handleCaptivePortal)
{
	if (running)
	{
		logger.log("WebServer: Already running");
		return true;
	}

	captivePortalHandler = handleCaptivePortal;

	// Set up routes
	server->onNotFound(std::bind(&SimpleWebServer::HandlePath, this));

	SetupOTA();

	// Diagnostics endpoints (plaintext / simple JSON-like output)
	server->on("/diagnostics/heap", std::bind(&SimpleWebServer::HandleHeapDiagnostics, this));
	server->on("/diagnostics/system", std::bind(&SimpleWebServer::HandleSystemDiagnostics, this));
	server->on("/diagnostics/network", std::bind(&SimpleWebServer::HandleNetworkDiagnostics, this));

	// Start the server
	server->begin();

	logger.logf("WebServer: Started on port %d", SERVER_PORT);

	running = true;

	return true;
}

void SimpleWebServer::Stop()
{
	if (!running)
	{
		return;
	}

	running = false;

	// Stop and delete server
	if (server != nullptr)
	{
		server->stop();
		delete server;
		server = nullptr;
	}

	logger.log("WebServer: Stopped");
}

// Helper method to set common headers
void SimpleWebServer::setCommonHeaders()
{
	server->sendHeader("Cache-Control", "no-cache");
	server->sendHeader("X-Content-Type-Options", "nosniff");
}

void SimpleWebServer::AddHandler(const Uri &uri, std::function<void()> handler)
{
	server->on(uri, handler);
}

void SimpleWebServer::AddHandler(const Uri &uri, HTTPMethod method, std::function<void()> handler)
{
	server->on(uri, method, handler);
}

bool SimpleWebServer::GetRequestArg(const char *name, String &value)
{
	if (!server->hasArg(name))
		return false;
	value = server->arg(name);
	return true;
}

void SimpleWebServer::HandlePath()
{
	//	logger.logf("HTTP request received for URI: %s", server->uri().c_str());

	// If the request is not for our server, then it was from the DNS capture so redirect to our IP and config page
	if (captivePortalHandler(server->hostHeader()))
		return;

	String uri = WebServer::urlDecode(server->uri()); // required to read paths with blanks
	RespondWithFileOr404(uri);
}

void SimpleWebServer::RespondWithFileOr404(String uri)
{
	// Only for Get and Post
	if (server->method() == HTTP_GET || server->method() == HTTP_POST)
	{
		// If request is for a dir, look for index.html in that dir
		if (uri.endsWith("/"))
			uri += "index.html";

		String contentType = mime::getContentType(uri);

		if (LittleFS.exists(uri))
		{
			// logger.logf("File found, sending response: %s", uri.c_str());
			if (uri.endsWith(".ttf") || uri.endsWith(".otf"))
			{
				// Special handling for fonts to allow caching
				server->sendHeader("X-Content-Type-Options", "nosniff");
				server->sendHeader("Cache-Control", "public, max-age=31536000"); // Cache for 1 year
			}
			else
				setCommonHeaders();

			File file = LittleFS.open(uri, "r");
			if (file)
			{
				server->streamFile(file, contentType);
				file.close();
				return;
			}
			logger.logf("File failed to open after finding it!");
		}
		logger.logf("File not found: %s", uri.c_str());
	}

	// File not found
	RespondWith404();
}

void SimpleWebServer::RespondWith404()
{
	setCommonHeaders();
	server->send(404, "text/plain; charset=utf-8", "File Not Found");
}

void SimpleWebServer::RespondWithContent(int responseCode, String response, String fileType)
{
	String contentType = mime::getContentType(fileType);
	setCommonHeaders();
	server->send(responseCode, contentType, response);
}

void SimpleWebServer::Redirect(String uri)
{
	setCommonHeaders();
	server->sendHeader("Location", uri, true);
	server->send(302, "text/plain; charset=utf-8", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
	server->client().stop();							// Stop is needed because we sent no content length
}

void SimpleWebServer::run()
{
	while (running)
	{
		uint32_t startLoop = micros();
		uint32_t hcStart = micros();
		if (server != nullptr)
		{
			// Handle ElegantOTA updates
			ElegantOTA.loop();

			server->handleClient();
			uint32_t hcElapsed = micros() - hcStart;
			handleClientMicrosAcc += hcElapsed;
			handleClientCalls++;
		}

		loopCount++;
		uint32_t now = millis();
		if (now - lastStatsMillis >= 1000)
		{
			loopsPerSecond = loopCount;
			loopCount = 0;
			handleClientMicrosAcc = 0; // Reset window accumulators after sampling endpoints can read them
			handleClientCalls = 0;
			lastStatsMillis = now;
		}

		// Short delay – tuneable. Reducing this (e.g. to 2ms) can improve latency at cost of CPU.
		vTaskDelay(pdMS_TO_TICKS(5));
	}
}

// ----------------------- Diagnostics -----------------------

void SimpleWebServer::HandleHeapDiagnostics()
{
	String out;
	out.reserve(256);
	out += "{\n";
	out += "  \"free_heap\": ";
	out += String(ESP.getFreeHeap());
	out += ",\n";
	out += "  \"min_free_heap\": ";
	out += String(ESP.getMinFreeHeap());
	out += ",\n";
	out += "  \"max_alloc_heap\": ";
	out += String(ESP.getMaxAllocHeap());
	out += ",\n";
#ifdef BOARD_HAS_PSRAM
	out += "  \"free_psram\": ";
	out += String(ESP.getFreePsram());
	out += ",\n";
#endif
	out += "  \"sketch_free_space\": ";
	out += String(ESP.getFreeSketchSpace());
	out += "\n";
	out += "}\n";
	RespondWithContent(200, out, ".json");
}

void SimpleWebServer::HandleSystemDiagnostics()
{
	// Grab snapshot (avoid inconsistency while building String)
	uint32_t loops = loopsPerSecond;
	uint32_t hcCalls = handleClientCalls;
	uint32_t hcMicros = handleClientMicrosAcc; // This will show zero if sampled just after rollover
	UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
	String out;
	out.reserve(320);
	out += "{\n";
	out += "  \"uptime_ms\": ";
	out += String(millis());
	out += ",\n";
	out += "  \"task_core\": ";
	out += String(xPortGetCoreID());
	out += ",\n";
	out += "  \"task_priority\": ";
	out += String(uxTaskPriorityGet(NULL));
	out += ",\n";
	out += "  \"stack_high_water_mark_words\": ";
	out += String(watermark);
	out += ",\n";
	out += "  \"loops_per_sec\": ";
	out += String(loops);
	out += ",\n";
	out += "  \"handle_client_calls\": ";
	out += String(hcCalls);
	out += ",\n";
	out += "  \"avg_handle_client_us\": ";
	if (hcCalls > 0)
		out += String((double)hcMicros / (double)hcCalls, 2);
	else
		out += "0";
	out += "\n";
	out += "}\n";
	RespondWithContent(200, out, ".json");
}

void SimpleWebServer::HandleNetworkDiagnostics()
{
	wl_status_t st = WiFi.status();
	String out;
	out.reserve(320);
	out += "{\n";
	out += "  \"status\": \"";
	switch (st)
	{
	case WL_CONNECTED:
		out += "connected";
		break;
	case WL_NO_SSID_AVAIL:
		out += "no_ssid";
		break;
	case WL_CONNECT_FAILED:
		out += "connect_failed";
		break;
	case WL_IDLE_STATUS:
		out += "idle";
		break;
	case WL_DISCONNECTED:
		out += "disconnected";
		break;
	default:
		out += "other";
		break;
	}
	out += "\",\n";
	out += "  \"ssid\": \"";
	out += WiFi.SSID();
	out += "\",\n";
	out += "  \"bssid\": \"";
	out += WiFi.BSSIDstr();
	out += "\",\n";
	out += "  \"rssi_dbm\": ";
	out += String(WiFi.RSSI());
	out += ",\n";
	out += "  \"ip\": \"";
	out += WiFi.localIP().toString();
	out += "\",\n";
	out += "  \"gateway\": \"";
	out += WiFi.gatewayIP().toString();
	out += "\",\n";
	out += "  \"subnet\": \"";
	out += WiFi.subnetMask().toString();
	out += "\"\n";
	out += "}\n";
	RespondWithContent(200, out, ".json");
}

void SimpleWebServer::onOTAStart()
{
	logger.log("OTA update process started.");
}

void SimpleWebServer::onOTAProgress(size_t current, size_t final)
{
	static size_t lastLogged = 0;
	// Log every 50KB to reduce log spam
	if (current - lastLogged >= 51200 || current == final)
	{
		logger.logf("OTA Progress: %u bytes", current);
		lastLogged = current;
	}
}

void SimpleWebServer::onOTAEnd(bool success)
{
	if (success)
		logger.log("OTA update completed successfully. Rebooting...");
	else
		logger.log("OTA update failed.");
}

void SimpleWebServer::SetupOTA()
{
#ifndef OTA_USERNAME
#error "OTA_USERNAME is not defined. Please define OTA_USERNAME and OTA_PASSWORD in the .env file."
#endif
#ifndef OTA_PASSWORD
#error "OTA_PASSWORD is not defined. Please define OTA_USERNAME and OTA_PASSWORD in the .env file."
#endif

	// Block access to /update - support only via direct scripts
	server->on("/update", HTTP_GET, [this]()
			   { server->send(403, "text/plain", "Access Denied. OTA updates disabled via web interface."); });

	// Initialize ElegantOTA - web-based OTA updates with authentication
	// Note: The webpage won't be accessible because our handler above takes precedence
	ElegantOTA.begin(server, OTA_USERNAME, OTA_PASSWORD);
	ElegantOTA.setAutoReboot(true);
	ElegantOTA.onStart([this]()
					   { onOTAStart(); });
	ElegantOTA.onProgress([this](size_t current, size_t total)
						  { onOTAProgress(current, total); });
	ElegantOTA.onEnd([this](bool success)
					 { onOTAEnd(success); });
	logger.logf("ElegantOTA enabled at /update for web-based firmware and LittleFS updates");
}
