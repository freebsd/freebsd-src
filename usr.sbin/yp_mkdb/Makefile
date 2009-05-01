# $FreeBSD: src/usr.sbin/yp_mkdb/Makefile,v 1.9.32.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../libexec/ypxfr ${.CURDIR}/../ypserv

PROG=	yp_mkdb
MAN=	yp_mkdb.8
SRCS=	yp_mkdb.c yp_dblookup.c yp_dbwrite.c

CFLAGS+= -Dyp_error=warnx
CFLAGS+= -I${.CURDIR}/../../libexec/ypxfr -I${.CURDIR}/../../usr.sbin/ypserv

.include <bsd.prog.mk>
