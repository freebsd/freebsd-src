/* $FreeBSD: src/sys/boot/ficl/unix.c,v 1.2.14.1 2010/12/21 17:10:29 kensmith Exp $ */

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


