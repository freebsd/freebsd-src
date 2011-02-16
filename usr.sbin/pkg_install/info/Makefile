# $FreeBSD: src/usr.sbin/pkg_install/info/Makefile,v 1.20.18.1.4.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	pkg_info
SRCS=	main.c perform.c show.c

CFLAGS+= -I${.CURDIR}/../lib

WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
