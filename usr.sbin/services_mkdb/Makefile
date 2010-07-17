# $FreeBSD: src/usr.sbin/services_mkdb/Makefile,v 1.1.2.2.2.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	services_mkdb
MAN=	services_mkdb.8
SRCS=	services_mkdb.c uniq.c

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.include <bsd.prog.mk>
