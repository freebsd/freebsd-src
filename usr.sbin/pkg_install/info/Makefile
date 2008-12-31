# $FreeBSD: src/usr.sbin/pkg_install/info/Makefile,v 1.20.26.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	pkg_info
SRCS=	main.c perform.c show.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
