#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/vmstat/Makefile,v 1.11.8.1 2005/08/19 00:07:06 rwatson Exp $

PROG=	vmstat
MAN=	vmstat.8
DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBMEMSTAT}
LDADD=	-ldevstat -lkvm -lmemstat

.include <bsd.prog.mk>
