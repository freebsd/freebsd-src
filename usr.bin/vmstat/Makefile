#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	vmstat
MAN=	vmstat.8
DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBMEMSTAT}
LDADD=	-ldevstat -lkvm -lmemstat

.include <bsd.prog.mk>
