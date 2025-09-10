#pragma once
#include <string>

class Log
{
public:
	virtual void onLog(const std::string &msg) = 0;
};
