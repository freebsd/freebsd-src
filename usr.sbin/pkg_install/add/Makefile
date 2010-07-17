# $FreeBSD: src/usr.sbin/pkg_install/add/Makefile,v 1.21.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	pkg_add
SRCS=	main.c perform.c futil.c extract.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	3
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
