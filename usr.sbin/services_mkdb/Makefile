# $FreeBSD: src/usr.sbin/services_mkdb/Makefile,v 1.1.2.2.4.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	services_mkdb
MAN=	services_mkdb.8
SRCS=	services_mkdb.c uniq.c

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.include <bsd.prog.mk>
