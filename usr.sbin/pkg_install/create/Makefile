# $FreeBSD: src/usr.sbin/pkg_install/create/Makefile,v 1.22.28.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	pkg_create
SRCS=	main.c perform.c pl.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	3
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBMD}
LDADD=	${LIBINSTALL} -lmd

.include <bsd.prog.mk>
