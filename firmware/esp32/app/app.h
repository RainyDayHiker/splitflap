#pragma once
#include "clock_task.h"
#include "webserver_task.h"
#include "wifi_task.h"

class App
{
public:
	App(SplitflapTask &splitflapTask, DisplayTask &displayTask, Logger &logger);

	void begin();

private:
	Clock clockTask;
	WebServerTask webServerTask;
	WiFiTask wifiTask;
};