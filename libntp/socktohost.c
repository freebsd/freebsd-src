/*
 * socktoa - return a numeric host name from a sockaddr_storage structure
 */
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <arpa/inet.h>

#include <stdio.h>

#include "ntp_fp.h"
#include "lib_strbuf.h"
#include "ntp_stdlib.h"
#include "ntp.h"


char *
socktohost(
	const sockaddr_u *sock
	)
{
	register char *buffer;

	LIB_GETBUF(buffer);
	if (getnameinfo(&sock->sa, SOCKLEN(sock), buffer,
	    LIB_BUFLENGTH, NULL, 0, 0))
		return stoa(sock);

	return buffer;
}
