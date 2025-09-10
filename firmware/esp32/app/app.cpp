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
}
