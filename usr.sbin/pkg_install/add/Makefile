# $FreeBSD: src/usr.sbin/pkg_install/add/Makefile,v 1.21.28.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	pkg_add
SRCS=	main.c perform.c futil.c extract.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	3
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
