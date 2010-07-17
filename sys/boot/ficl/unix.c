/* $FreeBSD: src/sys/boot/ficl/unix.c,v 1.2.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $ */

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


