#include "app.h"
#include "LocalTime.h"

App::App(SplitflapTask &splitflapTask, DisplayTask &displayTask, Logger &logger) : clockTask(splitflapTask, logger, 0),
																				   webServerTask(logger, 0),
																				   wifiTask(displayTask, webServerTask, logger, 0)
{
}

void App::begin()
{
	// Init Time
	LocalTime::setupTime(LocalTime::TimeZone::LosAngeles);
	// TODO: Setup config to store this

	wifiTask.Setup();

	webServerTask.Start(std::bind(&WiFiTask::HandleCaptivePortal, &wifiTask, std::placeholders::_1));
	webServerTask.begin();

	wifiTask.begin();

	clockTask.begin();
}
