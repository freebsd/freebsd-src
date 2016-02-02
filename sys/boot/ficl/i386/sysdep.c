/*******************************************************************
** s y s d e p . c
** Forth Inspired Command Language
** Author: John Sadler (john_sadler@alum.mit.edu)
** Created: 16 Oct 1997
** Implementations of FICL external interface functions... 
**
*******************************************************************/

/* $FreeBSD$ */

#ifdef TESTMAIN
#include <stdio.h>
#include <stdlib.h>
#else
#include <stand.h>
#ifdef __i386__
#include <machine/cpufunc.h>
#endif
#endif
#include "ficl.h"

/*
*******************  FreeBSD  P O R T   B E G I N S   H E R E ******************** Michael Smith
*/

#if PORTABLE_LONGMULDIV == 0
DPUNS ficlLongMul(FICL_UNS x, FICL_UNS y)
{
    DPUNS q;
    u_int64_t qx;

    qx = (u_int64_t)x * (u_int64_t) y;

    q.hi = (u_int32_t)( qx >> 32 );
    q.lo = (u_int32_t)( qx & 0xFFFFFFFFL);

    return q;
}

UNSQR ficlLongDiv(DPUNS q, FICL_UNS y)
{
    UNSQR result;
    u_int64_t qx, qh;

    qh = q.hi;
    qx = (qh << 32) | q.lo;

    result.quot = qx / y;
    result.rem  = qx % y;

    return result;
}
#endif

void  ficlTextOut(FICL_VM *pVM, char *msg, int fNewline)
{
    IGNORE(pVM);

    while(*msg != 0)
	putchar((unsigned char)*(msg++));
    if (fNewline)
	putchar('\n');

   return;
}

void *ficlMalloc (size_t size)
{
    return malloc(size);
}

void *ficlRealloc (void *p, size_t size)
{
    return realloc(p, size);
}

void  ficlFree   (void *p)
{
    free(p);
}

#ifndef TESTMAIN
#ifdef __i386__
/* 
 * outb ( port# c -- )
 * Store a byte to I/O port number port#
 */
void
ficlOutb(FICL_VM *pVM)
{
	u_char c;
	u_int32_t port;

	port=stackPopUNS(pVM->pStack);
	c=(u_char)stackPopINT(pVM->pStack);
	outb(port,c);
}

/*
 * inb ( port# -- c )
 * Fetch a byte from I/O port number port#
 */
void
ficlInb(FICL_VM *pVM)
{
	u_char c;
	u_int32_t port;

	port=stackPopUNS(pVM->pStack);
	c=inb(port);
	stackPushINT(pVM->pStack,c);
}
#endif
#endif

/*
** Stub function for dictionary access control - does nothing
** by default, user can redefine to guarantee exclusive dict
** access to a single thread for updates. All dict update code
** is guaranteed to be bracketed as follows:
** ficlLockDictionary(TRUE);
** <code that updates dictionary>
** ficlLockDictionary(FALSE);
**
** Returns zero if successful, nonzero if unable to acquire lock
** befor timeout (optional - could also block forever)
*/
#if FICL_MULTITHREAD
int ficlLockDictionary(short fLock)
{
	IGNORE(fLock);
	return 0;
}
#endif /* FICL_MULTITHREAD */


