# $FreeBSD: src/usr.sbin/pkg_install/delete/Makefile,v 1.22.28.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	pkg_delete
SRCS=	main.c perform.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBMD}
LDADD=	${LIBINSTALL} -lmd

.include <bsd.prog.mk>
