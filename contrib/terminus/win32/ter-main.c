#include <windows.h>

BOOL WINAPI DllMainCRTStartup(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	(void) hinstDLL;
	(void) fdwReason;
	(void) lpvReserved;
	return TRUE;
} 
