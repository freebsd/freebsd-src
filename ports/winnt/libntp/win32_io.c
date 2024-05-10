/* This file implements i/o calls that are specific to Windows */

#include <config.h>
#include <stdio.h>
#include "ntp_fp.h"
#include "ntp_net.h"
#include "ntp_stdlib.h"
#include "ntp_syslog.h"
#include "win32_io.h"
#include <isc/win32os.h>

/*
 * Define this macro to control the behavior of connection
 * resets on UDP sockets.  See Microsoft KnowledgeBase Article Q263823
 * for details.
 * Based on that article, it is surprising that a much newer winsock2.h
 * does not define SIO_UDP_CONNRESET (the one that comes with VS 2008).
 * NOTE: This requires that Windows 2000 systems install Service Pack 2
 * or later.
 */
#ifndef SIO_UDP_CONNRESET 
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12) 
#endif


void
InitSockets(
	void
	)
{
	static int done;
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	if (done) {
		return;
	}
	done = TRUE;

	/* Need Winsock 2.0 or better */
	wVersionRequested = MAKEWORD(2, 0);
 
	err = WSAStartup(wVersionRequested, &wsaData);
	if ( err != 0 ) {
		SetLastError(err);
		msyslog(LOG_ERR, "No usable winsock: %m");
		exit(1);
	}
}

/*
 * Windows 2000 SP2 and later systems cause UDP sockets using WASRecvFrom
 * to return a WSACONNRESET error when a prior WSASendTo on the socket
 * fails with an "ICMP port unreachable" response.  After the error the
 * socket is unusable and must be closed. We disable this behavior.
 * See Microsoft Knowledge Base Article Q263823 for details of this.
 * https://www.betaarchive.com/wiki/index.php?title=Microsoft_KB_Archive/263823
 * https://web.archive.org/web/20111205034113/support.microsoft.com/kb/263823
 */
void
connection_reset_fix(
	SOCKET		fd,
	sockaddr_u *	addr
	)
{
	DWORD dw;
	BOOL  bNewBehavior = FALSE;
	DWORD status;

	/*
	 * disable bad behavior using IOCTL: SIO_UDP_CONNRESET
	 * NT 4.0 has no problem
	 */
	if (isc_win32os_majorversion() >= 5) {
		status = WSAIoctl(fd, SIO_UDP_CONNRESET, &bNewBehavior,
				  sizeof(bNewBehavior), NULL, 0,
				  &dw, NULL, NULL);
		if (SOCKET_ERROR == status)
			msyslog(LOG_ERR,
				"connection_reset_fix() failed for address %s: %m", 
				stoa(addr));
	}
}

