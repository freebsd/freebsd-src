# $FreeBSD: src/usr.sbin/pkg_install/version/Makefile,v 1.16.24.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	pkg_version
SRCS=	main.c perform.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

test:
	sh ${.CURDIR}/test-pkg_version.sh

.include <bsd.prog.mk>
