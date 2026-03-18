#include "Game/App.hpp"

#define WIN32_LEAN_AND_MEAN		// Always #define this before #including <windows.h>
#include <windows.h>			// #include this (massive, platform-specific) header in VERY few places (and .CPPs only)

#define UNUSED(x) (void)(x);

//-----------------------------------------------------------------------------------------------
int WINAPI WinMain( HINSTANCE applicationInstanceHandle, HINSTANCE, LPSTR commandLineString, int )
{
	UNUSED( applicationInstanceHandle);
	UNUSED( commandLineString );

	g_app = new App();

	g_app->RunMainLoop();

	delete g_app;
	g_app = nullptr;

	return 0;
}


