# $FreeBSD: src/usr.sbin/pkg_install/updating/Makefile,v 1.2.6.2.2.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	pkg_updating
SRCS=	main.c

CFLAGS+= -I${.CURDIR}/../lib

WFORMAT?= 1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
