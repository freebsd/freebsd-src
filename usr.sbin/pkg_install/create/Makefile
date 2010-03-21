# $FreeBSD: src/usr.sbin/pkg_install/create/Makefile,v 1.22.32.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	pkg_create
SRCS=	main.c perform.c pl.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	3
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBMD}
LDADD=	${LIBINSTALL} -lmd

.include <bsd.prog.mk>
