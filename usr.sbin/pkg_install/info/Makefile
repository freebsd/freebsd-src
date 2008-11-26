# $FreeBSD: src/usr.sbin/pkg_install/info/Makefile,v 1.20.24.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	pkg_info
SRCS=	main.c perform.c show.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
