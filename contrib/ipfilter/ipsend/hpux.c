/*
 * (C)opyright 1997-1998 Darren Reed. (from tcplog)
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>


int	initdevice(device, sport, tout)
char	*device;
int	sport, tout;
{
	int	fd;

	if ((fd = socket(AF_DLI, SOCK_RAW, 0)) == -1)
		perror("socket");
	return fd;
}


/*
 * output an IP packet onto a fd opened for /dev/bpf
 */
int	sendip(fd, pkt, len)
int	fd, len;
char	*pkt;
{			
	if (send(fd, pkt, len, 0) == -1)
	    {
		perror("send");
		return -1;
	    }

	return len;
}


char *strdup(str)
char *str;
{
	char	*s;

	if ((s = (char *)malloc(strlen(str) + 1)))
		return strcpy(s, str);
	return NULL;
}
/*
 * (C)opyright 1997 Darren Reed. (from tcplog)
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>


int	initdevice(device, sport, tout)
char	*device;
int	sport, tout;
{
	int	fd;

	if ((fd = socket(AF_DLI, SOCK_RAW, 0)) == -1)
		perror("socket");
	return fd;
}


/*
 * output an IP packet onto a fd opened for /dev/bpf
 */
int	sendip(fd, pkt, len)
int	fd, len;
char	*pkt;
{			
	if (send(fd, pkt, len, 0) == -1)
	    {
		perror("send");
		return -1;
	    }

	return len;
}


char *strdup(str)
char *str;
{
	char	*s;

	if ((s = (char *)malloc(strlen(str) + 1)))
		return strcpy(s, str);
	return NULL;
}
