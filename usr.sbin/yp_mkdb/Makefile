# $FreeBSD: src/usr.sbin/yp_mkdb/Makefile,v 1.8 2001/07/20 06:20:27 obrien Exp $

.PATH: ${.CURDIR}/../../libexec/ypxfr ${.CURDIR}/../ypserv

PROG=	yp_mkdb
MAN=	yp_mkdb.8
SRCS=	yp_mkdb.c yp_dblookup.c yp_dbwrite.c

CFLAGS+= -Dyp_error=warnx -I${.CURDIR}/../../libexec/ypxfr
CFLAGS+= -I${.CURDIR}/../../usr.sbin/ypserv

.include <bsd.prog.mk>
