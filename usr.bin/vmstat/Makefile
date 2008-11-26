#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/vmstat/Makefile,v 1.11.8.2.2.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	vmstat
MAN=	vmstat.8
DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBMEMSTAT} ${LIBUTIL}
LDADD=	-ldevstat -lkvm -lmemstat -lutil

.include <bsd.prog.mk>
