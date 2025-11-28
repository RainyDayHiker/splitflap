#pragma once
#include <deque>
#include "SimpleWebServer.h"
#include "Network.h"
#include "Config.h"
#include "Log.h"
#include "SplitFlap.h"
#include "SplitFlapComposer.h"

class App : public Log
{
public:
	App(SplitflapTask &splitflapTask, DisplayTask &displayTask, Logger &logger);

	void begin();

	// Log interface implementation
	void onLog(const std::string &msg) override;
	std::vector<std::string> getRecentLogs() const;

private:
	SimpleWebServer simpleWebServer;
	Network network;
	Config config;
	SplitFlap splitFlap;
	SplitFlapComposer splitFlapComposer;

	void registerHandlers();

	std::deque<std::string> logBuffer_;
	static constexpr size_t LOG_BUFFER_SIZE = 15;
};