#include "webserver_task.h"
#include <Arduino.h>
#include <LittleFS.h>

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
		{".html", "text/html"},
		{".htm", "text/html"},
		{".txt", "text/plain"},
		{".css", "text/css"},
		{".js", "application/javascript"},
		{".json", "application/json"},
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
		{".appcache", "text/cache-manifest"},
		{".gz", "application/gzip"},
		{".none", "application/octet-stream"}};

	String getContentType(const String &path)
	{
		for (size_t i = 0; i < maxType; i++)
		{
			if (path.endsWith(FPSTR(mimeTable[i].endsWith)))
			{
				return String(FPSTR(mimeTable[i].mimeType));
			}
		}
		// Fall-through and just return default type
		return String(FPSTR(mimeTable[none].mimeType));
	}
}

WebServerTask::WebServerTask(Logger &logger, const uint8_t task_core) : Task("WebServer", 4096, 1, task_core),
																		logger_(logger),
																		server(nullptr),
																		running(false)
{
	server = new WebServer(SERVER_PORT);
}

WebServerTask::~WebServerTask()
{
	Stop();
}

bool WebServerTask::Start(std::function<bool(String)> handleCaptivePortal)
{
	if (running)
	{
		logger_.log("WebServer: Already running");
		return true;
	}

	captivePortalHandler = handleCaptivePortal;

	// Set up routes
	server->onNotFound(std::bind(&WebServerTask::HandlePath, this));

	// Start the server
	server->begin();

	char msg[100];
	snprintf(msg, sizeof(msg), "WebServer: Started on port %d", SERVER_PORT);
	logger_.log(msg);

	snprintf(msg, sizeof(msg), "WebServer: Access at http://%s/", WiFi.localIP().toString().c_str());
	logger_.log(msg);

	running = true;

	return true;
}

void WebServerTask::Stop()
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

	logger_.log("WebServer: Stopped");
}

// Helper method to set common headers
void WebServerTask::setCommonHeaders()
{
	server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate, max-age=0");
	server->sendHeader("X-Content-Type-Options", "nosniff");
}

void WebServerTask::AddHandler(const Uri &uri, std::function<void()> handler)
{
	server->on(uri, handler);
}

void WebServerTask::AddHandler(const Uri &uri, HTTPMethod method, std::function<void()> handler)
{
	server->on(uri, method, handler);
}

bool WebServerTask::GetRequestArg(const char *name, String &value)
{
	if (!server->hasArg(name))
		return false;
	value = server->arg(name);
	return true;
}

void WebServerTask::HandlePath()
{
	char buf[200];
	snprintf(buf, sizeof(buf), "HTTP request received for URI: %s", server->uri().c_str());
	logger_.log(buf);

	// If the request is not for our server, then it was from the DNS capture so redirect to our IP and config page
	if (captivePortalHandler(server->hostHeader()))
		return;

	String uri = WebServer::urlDecode(server->uri()); // required to read paths with blanks
	RespondWithFileOr404(uri);
}

void WebServerTask::RespondWithFileOr404(String uri)
{
	// Only for Get and Post
	if (server->method() == HTTP_GET || server->method() == HTTP_POST)
	{
		// If request is for a dir, look for index.html in that dir
		if (uri.endsWith("/"))
			uri += "index.html";

		String contentType = mime::getContentType(uri);
		// Add charset=utf-8 for text content types
		if (contentType.startsWith("text/"))
			contentType += "; charset=utf-8";

		char buf[200];
		snprintf(buf, sizeof(buf), "Looking for file: %s", uri.c_str());
		logger_.log(buf);

		if (LittleFS.exists(uri))
		{
			logger_.log("File found, sending response");

			setCommonHeaders();

			File file = LittleFS.open(uri, "r");
			server->streamFile(file, contentType);
			file.close();
			return;
		}
		logger_.log("File not found");
	}

	// File not found
	RespondWith404();
}

void WebServerTask::RespondWith404()
{
	setCommonHeaders();
	server->send(404, "text/plain; charset=utf-8", "File Not Found");
}

void WebServerTask::RespondWithContent(int responseCode, String response)
{
	setCommonHeaders();
	server->send(responseCode, "text/plain; charset=utf-8", response);
}

void WebServerTask::Redirect(String uri)
{
	setCommonHeaders();
	server->sendHeader("Location", uri, true);
	server->send(302, "text/plain; charset=utf-8", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
	server->client().stop();							// Stop is needed because we sent no content length
}

void WebServerTask::run()
{
	while (running)
	{
		if (server != nullptr)
		{
			server->handleClient();
		}
		vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent watchdog issues
	}
}
