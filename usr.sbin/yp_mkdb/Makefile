# $Id: Makefile,v 1.1.1.1 1996/04/28 04:16:05 wpaul Exp $

PROG=	yp_mkdb
SRCS=	yp_mkdb.c yp_dblookup.c yp_dbwrite.c

MAN8=	yp_mkdb.8

.PATH:	${.CURDIR}/../../libexec/ypxfr ${.CURDIR}/../ypserv

CFLAGS+= -Dyp_error=warnx -I${.CURDIR}/../../libexec/ypxfr
CFLAGS+= -I${.CURDIR}/../../usr.sbin/ypserv

.include <bsd.prog.mk>
