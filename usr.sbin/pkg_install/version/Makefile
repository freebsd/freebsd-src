# $FreeBSD: src/usr.sbin/pkg_install/version/Makefile,v 1.5.2.7 2004/12/13 13:50:01 marius Exp $

PROG=	pkg_version
SRCS=	main.c perform.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
DPADD+=	${LIBSSL} ${LIBCRYPTO}
LDADD+=	-lssl -lcrypto
.endif

test:
	sh ${.CURDIR}/test-pkg_version.sh

.include <bsd.prog.mk>
