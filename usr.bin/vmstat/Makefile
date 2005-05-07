#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/vmstat/Makefile,v 1.11 2004/02/04 10:20:43 ru Exp $

PROG=	vmstat
MAN=	vmstat.8
DPADD=	${LIBDEVSTAT} ${LIBKVM}
LDADD=	-ldevstat -lkvm

.include <bsd.prog.mk>
