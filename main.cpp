#include "Logger.h"
#include "MainApp.h"
#include "Utils.h"

using namespace Lunar;

int main()
{
	try
	{
		Lunar::MainApp mainApp;
		mainApp.Initialize();
		return mainApp.Run();
	}
	catch (Lunar::LunarException& e)
	{
		LOG_ERROR(e.ToString());
		return 0;
	}
}
