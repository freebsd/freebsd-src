/*
 * socktoa - return a numeric host name from a sockaddr_storage structure
 */

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

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
	struct sockaddr_storage* sock
	)
{
	register char *buffer;

	LIB_GETBUF(buffer);

	if (sock == NULL) printf("null");

	switch(sock->ss_family) {

		case AF_INET :
			inet_ntop(AF_INET, &GET_INADDR(*sock), buffer,
			    LIB_BUFLENGTH);
			break;

		case AF_INET6 :
			inet_ntop(AF_INET6, &GET_INADDR6(*sock), buffer,
			    LIB_BUFLENGTH);
	}
  	return buffer;
}
