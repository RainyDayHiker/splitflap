#include "app.h"
#include "LocalTime.h"

App::App(SplitflapTask &splitflapTask, DisplayTask &displayTask, Logger &logger) : clockTask(splitflapTask, logger),
																				   simpleWebServer(logger),
																				   network(displayTask, simpleWebServer, logger),
																				   config(simpleWebServer, logger)
{
}

void App::begin()
{
	config.Setup();

	LocalTime::setupTime(config.GetTimeZone());

	network.Setup();

	simpleWebServer.Start(std::bind(&Network::HandleCaptivePortal, &network, std::placeholders::_1));
	simpleWebServer.begin();

	network.begin();

	clockTask.begin();

	config.begin();

	// Add /logs endpoint to webserver
	simpleWebServer.AddHandler("/logs", [this]()
							   {
		std::vector<std::string> logs = getRecentLogs();
		String json = "[";
		for (size_t i = 0; i < logs.size(); ++i) {
			// Escape quotes and backslashes for JSON
			String logStr = String(logs[i].c_str());
			logStr.replace("\\", "\\\\");
			logStr.replace("\"", "\\\"");
			json += "\"" + logStr + "\"";
			if (i < logs.size() - 1) json += ",";
		}
		json += "]";
		simpleWebServer.RespondWithContent(200, json); });
}

void App::onLog(const std::string &msg)
{
	if (logBuffer_.size() >= LOG_BUFFER_SIZE)
	{
		logBuffer_.pop_front();
	}
	logBuffer_.push_back(msg);
}

std::vector<std::string> App::getRecentLogs() const
{
	return std::vector<std::string>(logBuffer_.begin(), logBuffer_.end());
}
