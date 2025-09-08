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
	bool Start(std::function<bool(String)> handleCaptivePortal);

	// Stop the webserver
	void Stop();

	void AddHandler(const Uri &uri, std::function<void()> handler);
	void AddHandler(const Uri &uri, HTTPMethod method, std::function<void()> handler);

	bool GetRequestArg(const char *name, String &value);

	void RespondWithFileOr404(String uri);
	void RespondWith404();
	void RespondWithContent(int responseCode, String response);
	void Redirect(String uri);

protected:
	void run();

private:
	void setCommonHeaders(); // Helper method to set security headers

	void HandlePath();

	Logger &logger_;
	WebServer *server;
	bool running;
	static const uint16_t SERVER_PORT = 80;
	std::function<bool(String)> captivePortalHandler;
};

#endif // WEBSERVER_TASK_H