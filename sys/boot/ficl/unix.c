/* $FreeBSD: src/sys/boot/ficl/unix.c,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $ */

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


