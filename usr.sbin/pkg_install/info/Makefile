# $FreeBSD: src/usr.sbin/pkg_install/info/Makefile,v 1.20.28.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	pkg_info
SRCS=	main.c perform.c show.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
