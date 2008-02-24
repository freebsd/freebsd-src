#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/vmstat/Makefile,v 1.12 2005/07/25 22:35:10 rwatson Exp $

PROG=	vmstat
MAN=	vmstat.8
DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBMEMSTAT}
LDADD=	-ldevstat -lkvm -lmemstat

.include <bsd.prog.mk>
