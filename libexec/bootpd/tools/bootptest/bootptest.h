/* bootptest.h */
/* $FreeBSD: src/libexec/bootpd/tools/bootptest/bootptest.h,v 1.2.32.1 2008/11/25 02:59:29 kensmith Exp $ */
/*
 * Hacks for sharing print-bootp.c between tcpdump and bootptest.
 */
#define ESRC(p) (p)
#define EDST(p) (p)

#ifndef	USE_BFUNCS
/* Use mem/str functions */
/* There are no overlapped copies, so memcpy is OK. */
#define bcopy(a,b,c)    memcpy(b,a,c)
#define bzero(p,l)      memset(p,0,l)
#define bcmp(a,b,c)     memcmp(a,b,c)
#endif

extern int vflag; /* verbose flag */

/* global pointers to beginning and end of current packet (during printing) */
extern unsigned char *packetp;
extern unsigned char *snapend;

extern char *ipaddr_string(struct in_addr *);
