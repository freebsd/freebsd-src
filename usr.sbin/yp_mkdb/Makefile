# $Id$

PROG=	yp_mkdb
SRCS=	yp_mkdb.c yp_dblookup.c yp_dbwrite.c

MAN8=	yp_mkdb.8

.PATH:	${.CURDIR}/../../libexec/ypxfr ${.CURDIR}/../ypserv

CFLAGS+= -Dyp_error=warnx
CFLAGS+= -I${.CURDIR}/../../usr.sbin/ypserv

.include <bsd.prog.mk>
