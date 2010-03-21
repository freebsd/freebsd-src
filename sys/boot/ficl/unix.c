/* $FreeBSD: src/sys/boot/ficl/unix.c,v 1.2.12.1 2010/02/10 00:26:20 kensmith Exp $ */

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


