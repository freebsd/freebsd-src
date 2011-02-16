# $FreeBSD: src/usr.sbin/yp_mkdb/Makefile,v 1.9.34.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../libexec/ypxfr ${.CURDIR}/../ypserv

PROG=	yp_mkdb
MAN=	yp_mkdb.8
SRCS=	yp_mkdb.c yp_dblookup.c yp_dbwrite.c

CFLAGS+= -Dyp_error=warnx
CFLAGS+= -I${.CURDIR}/../../libexec/ypxfr -I${.CURDIR}/../../usr.sbin/ypserv

.include <bsd.prog.mk>
