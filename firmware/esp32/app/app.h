#pragma once
#include "Clock.h"
#include "SimpleWebServer.h"
#include "Network.h"
#include "Config.h"

class App
{
public:
	App(SplitflapTask &splitflapTask, DisplayTask &displayTask, Logger &logger);

	void begin();

private:
	Clock clockTask;
	SimpleWebServer simpleWebServer;
	Network network;
	Config config;
};