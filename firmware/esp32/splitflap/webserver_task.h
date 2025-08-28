#ifndef WEBSERVER_TASK_H
#define WEBSERVER_TASK_H

#include <WiFi.h>
#include <WebServer.h>
#include "../core/logger.h"
#include "../core/task.h"

class WebServerTask : public Task<WebServerTask>
{
	friend class Task<WebServerTask>; // Allow base Task to invoke protected run()

public:
	WebServerTask(Logger &logger, const uint8_t task_core);
	~WebServerTask();

	// Initialize and start the webserver
	bool begin();

	// Stop the webserver
	void stop();

	// Check if webserver is running
	bool isRunning() const;

	// Get server port
	uint16_t getPort() const;

protected:
	void run();

private:
	void handleRoot();
	void handleNotFound();
	void handleStatus();
	void handleAPI();

	Logger &logger_;
	WebServer *server;
	bool running;
	static const uint16_t SERVER_PORT = 80;
};

#endif // WEBSERVER_TASK_H