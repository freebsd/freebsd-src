# $FreeBSD: src/usr.sbin/pkg_install/delete/Makefile,v 1.11.2.5 2004/08/11 16:16:23 eik Exp $

PROG=	pkg_delete
SRCS=	main.c perform.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBMD}
LDADD=	${LIBINSTALL} -lmd

.include <bsd.prog.mk>
