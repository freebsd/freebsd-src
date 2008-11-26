# $FreeBSD: src/usr.sbin/pkg_install/updating/Makefile,v 1.2.4.1.2.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	pkg_updating
SRCS=	main.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?= 1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
