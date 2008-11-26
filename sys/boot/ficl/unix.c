/* $FreeBSD: src/sys/boot/ficl/unix.c,v 1.1.26.1 2008/10/02 02:57:24 kensmith Exp $ */

#include <string.h>
#include <netinet/in.h>

#include "ficl.h"



unsigned long ficlNtohl(unsigned long number)
	{
	return ntohl(number);
	}




void ficlCompilePlatform(FICL_DICT *dp)
{
    return;
}


