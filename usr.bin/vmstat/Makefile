#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/vmstat/Makefile,v 1.12.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	vmstat
MAN=	vmstat.8
DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBMEMSTAT} ${LIBUTIL}
LDADD=	-ldevstat -lkvm -lmemstat -lutil

.include <bsd.prog.mk>
