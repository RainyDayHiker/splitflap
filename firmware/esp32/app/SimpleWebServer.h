#pragma once

#include <WebServer.h>
#include "../core/logger.h"
#include "../core/task.h"
#include "Network.h"

class SimpleWebServer : public Task<SimpleWebServer>
{
	friend class Task<SimpleWebServer>; // Allow base Task to invoke protected run()

public:
	SimpleWebServer(Logger &logger, const uint8_t task_core = 0);
	~SimpleWebServer();

	// Initialize and start the webserver
	bool Start(std::function<bool(String)> handleCaptivePortal);

	// Stop the webserver
	void Stop();

	void AddHandler(const Uri &uri, std::function<void()> handler);
	void AddHandler(const Uri &uri, HTTPMethod method, std::function<void()> handler);

	bool GetRequestArg(const char *name, String &value);

	void RespondWithFileOr404(String uri);
	void RespondWith404();
	void RespondWithContent(int responseCode, String response, String fileType = ".txt");
	void RespondWithContentChunked(int responseCode, const String &response, String fileType = ".txt");
	void Redirect(String uri);

	void SetupOTA();

protected:
	void run();

private:
	void setCommonHeaders(); // Helper method to set security headers
	void HandlePath();

	// Diagnostics handlers
	void HandleHeapDiagnostics();
	void HandleSystemDiagnostics();
	void HandleNetworkDiagnostics();

	void onOTAStart();
	void onOTAProgress(size_t current, size_t final);
	void onOTAEnd(bool success);

	// Internal perf counters (updated in run loop)
	volatile uint32_t loopCount = 0;			 // Loops counted within current 1s window
	volatile uint32_t loopsPerSecond = 0;		 // Last computed loops/sec
	volatile uint32_t handleClientCalls = 0;	 // Number of handleClient() calls in window
	volatile uint32_t handleClientMicrosAcc = 0; // Accumulated time spent in handleClient in microseconds (window)
	uint32_t lastStatsMillis = 0;				 // For 1s window rollover

	Logger &logger;
	WebServer *server;
	bool running;
	static const uint16_t SERVER_PORT = 80;
	std::function<bool(String)> captivePortalHandler;
};
