# $FreeBSD: src/usr.sbin/pkg_install/delete/Makefile,v 1.22.18.1.2.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	pkg_delete
SRCS=	main.c perform.c

CFLAGS+= -I${.CURDIR}/../lib

WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBMD}
LDADD=	${LIBINSTALL} -lmd

.include <bsd.prog.mk>
