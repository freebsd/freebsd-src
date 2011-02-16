# $FreeBSD: src/usr.sbin/pkg_install/version/Makefile,v 1.16.18.1.4.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	pkg_version
SRCS=	main.c perform.c

CFLAGS+= -I${.CURDIR}/../lib

WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

test:
	sh ${.CURDIR}/test-pkg_version.sh

.include <bsd.prog.mk>
