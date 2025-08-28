#include "webserver_task.h"
#include <Arduino.h>

WebServerTask::WebServerTask(Logger &logger, const uint8_t task_core) : Task("WebServer", 4096, 1, task_core),
																		logger_(logger),
																		server(nullptr),
																		running(false)
{
}

WebServerTask::~WebServerTask()
{
	stop();
}

bool WebServerTask::begin()
{
	if (running)
	{
		logger_.log("WebServer: Already running");
		return true;
	}

	// Check if WiFi is connected
	if (WiFi.status() != WL_CONNECTED)
	{
		logger_.log("WebServer: WiFi not connected");
		return false;
	}

	// Create WebServer instance
	server = new WebServer(SERVER_PORT);

	// Set up routes
	server->on("/", [this]()
			   { handleRoot(); });
	server->on("/status", [this]()
			   { handleStatus(); });
	server->on("/api", [this]()
			   { handleAPI(); });
	server->onNotFound([this]()
					   { handleNotFound(); });

	// Start the server
	server->begin();

	char msg[100];
	snprintf(msg, sizeof(msg), "WebServer: Started on port %d", SERVER_PORT);
	logger_.log(msg);

	snprintf(msg, sizeof(msg), "WebServer: Access at http://%s/", WiFi.localIP().toString().c_str());
	logger_.log(msg);

	running = true;

	// Start the FreeRTOS task (using the new Task pattern)
	Task<WebServerTask>::begin();

	return true;
}

void WebServerTask::stop()
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

bool WebServerTask::isRunning() const
{
	return running;
}

uint16_t WebServerTask::getPort() const
{
	return SERVER_PORT;
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

void WebServerTask::handleRoot()
{
	String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Splitflap Display</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { 
            font-family: Arial, sans-serif; 
            margin: 20px; 
            background-color: #f0f0f0; 
        }
        .container { 
            max-width: 800px; 
            margin: 0 auto; 
            background: white; 
            padding: 20px; 
            border-radius: 8px; 
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        h1 { 
            color: #333; 
            text-align: center; 
        }
        .status { 
            background: #e8f5e8; 
            padding: 10px; 
            border-radius: 4px; 
            margin: 20px 0; 
        }
        .nav { 
            text-align: center; 
            margin: 20px 0; 
        }
        .nav a { 
            display: inline-block; 
            margin: 0 10px; 
            padding: 10px 20px; 
            background: #007bff; 
            color: white; 
            text-decoration: none; 
            border-radius: 4px; 
        }
        .nav a:hover { 
            background: #0056b3; 
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Splitflap Display Control</h1>
        <div class="status">
            <h3>Device Status</h3>
            <p><strong>IP Address:</strong> )" +
				  WiFi.localIP().toString() + R"(</p>
            <p><strong>MAC Address:</strong> )" +
				  WiFi.macAddress() + R"(</p>
            <p><strong>RSSI:</strong> )" +
				  String(WiFi.RSSI()) + R"( dBm</p>
            <p><strong>Free Heap:</strong> )" +
				  String(ESP.getFreeHeap()) + R"( bytes</p>
            <p><strong>Uptime:</strong> )" +
				  String(millis() / 1000) + R"( seconds</p>
        </div>
        <div class="nav">
            <a href="/status">Status JSON</a>
            <a href="/api">API Info</a>
        </div>
        <div>
            <h3>About</h3>
            <p>This is a basic web interface for the Splitflap Display device. 
               Use the navigation links above to access different functions.</p>
        </div>
    </div>
</body>
</html>
)";

	server->send(200, "text/html", html);
}

void WebServerTask::handleStatus()
{
	String json = "{\n";
	json += "  \"device\": \"Splitflap Display\",\n";
	json += "  \"ip\": \"" + WiFi.localIP().toString() + "\",\n";
	json += "  \"mac\": \"" + WiFi.macAddress() + "\",\n";
	json += "  \"rssi\": " + String(WiFi.RSSI()) + ",\n";
	json += "  \"freeHeap\": " + String(ESP.getFreeHeap()) + ",\n";
	json += "  \"uptime\": " + String(millis() / 1000) + ",\n";
	json += "  \"running\": " + String(running ? "true" : "false") + "\n";
	json += "}";

	server->send(200, "application/json", json);
}

void WebServerTask::handleAPI()
{
	String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>API Documentation</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }
        .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; }
        .endpoint { background: #f8f9fa; padding: 15px; margin: 10px 0; border-radius: 4px; border-left: 4px solid #007bff; }
        .method { display: inline-block; padding: 2px 8px; background: #28a745; color: white; border-radius: 3px; font-weight: bold; }
    </style>
</head>
<body>
    <div class="container">
        <h1>API Documentation</h1>
        <p>Available endpoints for the Splitflap Display API:</p>
        
        <div class="endpoint">
            <h3><span class="method">GET</span> /</h3>
            <p>Main web interface with device status and controls</p>
        </div>
        
        <div class="endpoint">
            <h3><span class="method">GET</span> /status</h3>
            <p>Returns device status in JSON format</p>
        </div>
        
        <div class="endpoint">
            <h3><span class="method">GET</span> /api</h3>
            <p>This API documentation page</p>
        </div>
        
        <p><a href="/">← Back to main page</a></p>
    </div>
</body>
</html>
)";

	server->send(200, "text/html", html);
}

void WebServerTask::handleNotFound()
{
	String message = "File Not Found\n\n";
	message += "URI: " + server->uri() + "\n";
	message += "Arguments: " + String(server->args()) + "\n";

	for (uint8_t i = 0; i < server->args(); i++)
	{
		message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
	}

	server->send(404, "text/plain", message);
}