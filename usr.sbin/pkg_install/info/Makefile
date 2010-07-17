# $FreeBSD: src/usr.sbin/pkg_install/info/Makefile,v 1.20.30.2.2.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	pkg_info
SRCS=	main.c perform.c show.c

CFLAGS+= -I${.CURDIR}/../lib

WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
