# $FreeBSD: src/usr.sbin/pkg_install/create/Makefile,v 1.22.30.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	pkg_create
SRCS=	main.c perform.c pl.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	3
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBMD}
LDADD=	${LIBINSTALL} -lmd

.include <bsd.prog.mk>
