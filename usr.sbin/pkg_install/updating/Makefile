# $FreeBSD: src/usr.sbin/pkg_install/updating/Makefile,v 1.2.2.2.4.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	pkg_updating
SRCS=	main.c

CFLAGS+= -I${.CURDIR}/../lib

WFORMAT?= 1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
