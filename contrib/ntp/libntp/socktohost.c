/*
 * socktoa - return a numeric host name from a sockaddr_storage structure
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include <arpa/inet.h>

#include <stdio.h>

#include "ntp_fp.h"
#include "lib_strbuf.h"
#include "ntp_stdlib.h"
#include "ntp.h"


char *
socktohost(
	struct sockaddr_storage* sock
	)
{
	register char *buffer;

	LIB_GETBUF(buffer);
	if (getnameinfo((struct sockaddr *)sock, SOCKLEN(sock), buffer,
	    LIB_BUFLENGTH /* NI_MAXHOST*/, NULL, 0, 0))
		return stoa(sock);

  	return buffer;
}
