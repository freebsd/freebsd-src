# $FreeBSD: src/usr.sbin/pkg_install/create/Makefile,v 1.11.2.4 2004/08/11 16:16:22 eik Exp $

PROG=	pkg_create
SRCS=	main.c perform.c pl.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	3
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBMD}
LDADD=	${LIBINSTALL} -lmd

.include <bsd.prog.mk>
