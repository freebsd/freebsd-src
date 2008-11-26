# $FreeBSD: src/usr.sbin/pkg_install/delete/Makefile,v 1.22.24.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	pkg_delete
SRCS=	main.c perform.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBMD}
LDADD=	${LIBINSTALL} -lmd

.include <bsd.prog.mk>
