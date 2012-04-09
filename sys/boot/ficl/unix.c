/* $FreeBSD: src/sys/boot/ficl/unix.c,v 1.2.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $ */

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


