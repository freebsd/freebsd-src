# $FreeBSD: src/usr.sbin/pkg_install/delete/Makefile,v 1.22.30.2.2.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	pkg_delete
SRCS=	main.c perform.c

CFLAGS+= -I${.CURDIR}/../lib

WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBMD}
LDADD=	${LIBINSTALL} -lmd

.include <bsd.prog.mk>
