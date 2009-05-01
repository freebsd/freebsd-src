# $FreeBSD: src/usr.sbin/pkg_install/updating/Makefile,v 1.2.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	pkg_updating
SRCS=	main.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?= 1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
