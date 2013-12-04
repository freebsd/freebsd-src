/*
 * socktoa - return a numeric host name from a sockaddr_storage structure
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <arpa/inet.h>

#ifdef ISC_PLATFORM_NEEDNTOP
#include <isc/net.h>
#endif

#include <stdio.h>

#include "ntp_fp.h"
#include "lib_strbuf.h"
#include "ntp_stdlib.h"
#include "ntp.h"

char *
socktoa(
	const sockaddr_u *sock
	)
{
	register char *buffer;

	LIB_GETBUF(buffer);

	if (NULL == sock)
		strncpy(buffer, "(null)", LIB_BUFLENGTH);
	else {
		switch(AF(sock)) {

		case AF_INET:
		case AF_UNSPEC:
			inet_ntop(AF_INET, PSOCK_ADDR4(sock), buffer,
				  LIB_BUFLENGTH);
			break;

		case AF_INET6:
			inet_ntop(AF_INET6, PSOCK_ADDR6(sock), buffer,
				  LIB_BUFLENGTH);
			break;

		default:
			snprintf(buffer, LIB_BUFLENGTH, 
				 "(socktoa unknown family %d)", 
				 AF(sock));
		}
	}
	return buffer;
}
