#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <locale.h>

void main(int argc, char **argv) {

	HINSTANCE hinst;
	WCHAR buffer[128];
	unsigned char winbuf[128],oembuf[128];
	unsigned int number;

	if (argc <3)
		return;

   	hinst = LoadLibrary(argv[1]);

	number = atoi(argv[2]);
	printf("Load String returns %i\n",	
		LoadStringW(hinst, number, buffer, sizeof(buffer)));

	WideCharToMultiByte(CP_OEMCP,
						0,
						buffer,
						-1,
						winbuf,
						128,
						NULL,
						NULL);

	CharToOem(winbuf,oembuf);
	printf("oem: %s\n",oembuf);
}
