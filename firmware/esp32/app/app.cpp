#include "app.h"
#include "LocalTime.h"

App::App(SplitflapTask &splitflapTask, DisplayTask &displayTask, Logger &logger) : clockTask(splitflapTask, logger),
																				   simpleWebServer(logger),
																				   network(displayTask, simpleWebServer, logger)
{
}

void App::begin()
{
	// Init Time
	LocalTime::setupTime(LocalTime::TimeZone::LosAngeles);
	// TODO: Setup config to store this

	network.Setup();

	simpleWebServer.Start(std::bind(&Network::HandleCaptivePortal, &network, std::placeholders::_1));
	simpleWebServer.begin();

	network.begin();

	clockTask.begin();
}
