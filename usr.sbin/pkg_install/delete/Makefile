# $FreeBSD: src/usr.sbin/pkg_install/delete/Makefile,v 1.22 2004/08/12 20:06:00 ru Exp $

PROG=	pkg_delete
SRCS=	main.c perform.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBMD}
LDADD=	${LIBINSTALL} -lmd

.include <bsd.prog.mk>
