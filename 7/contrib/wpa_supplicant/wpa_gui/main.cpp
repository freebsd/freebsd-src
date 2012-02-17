#ifdef CONFIG_NATIVE_WINDOWS
#include <winsock.h>
#endif /* CONFIG_NATIVE_WINDOWS */
#include <qapplication.h>
#include "wpagui.h"

int main( int argc, char ** argv )
{
    QApplication a( argc, argv );
    WpaGui w;
    int ret;

#ifdef CONFIG_NATIVE_WINDOWS
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 0), &wsaData)) {
	printf("Could not find a usable WinSock.dll\n");
	return -1;
    }
#endif /* CONFIG_NATIVE_WINDOWS */

    w.show();
    a.connect( &a, SIGNAL( lastWindowClosed() ), &a, SLOT( quit() ) );
    ret = a.exec();

#ifdef CONFIG_NATIVE_WINDOWS
    WSACleanup();
#endif /* CONFIG_NATIVE_WINDOWS */

    return ret;
}
