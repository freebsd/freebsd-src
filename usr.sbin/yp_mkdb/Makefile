# $FreeBSD: src/usr.sbin/yp_mkdb/Makefile,v 1.9.34.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../libexec/ypxfr ${.CURDIR}/../ypserv

PROG=	yp_mkdb
MAN=	yp_mkdb.8
SRCS=	yp_mkdb.c yp_dblookup.c yp_dbwrite.c

CFLAGS+= -Dyp_error=warnx
CFLAGS+= -I${.CURDIR}/../../libexec/ypxfr -I${.CURDIR}/../../usr.sbin/ypserv

.include <bsd.prog.mk>
