# $FreeBSD: src/usr.sbin/services_mkdb/Makefile,v 1.1.2.2.6.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	services_mkdb
MAN=	services_mkdb.8
SRCS=	services_mkdb.c uniq.c

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.include <bsd.prog.mk>
