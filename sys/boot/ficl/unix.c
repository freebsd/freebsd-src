/* $FreeBSD: src/sys/boot/ficl/unix.c,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $ */

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


