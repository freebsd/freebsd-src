# $FreeBSD: src/usr.sbin/pkg_install/version/Makefile,v 1.12 2003/05/19 15:52:01 des Exp $

PROG=	pkg_version
SRCS=	main.c perform.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	2

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
DPADD+=	${LIBSSL} ${LIBCRYPTO}
LDADD+=	-lssl -lcrypto
.endif

test:
	./test-pkg_version.sh

.include <bsd.prog.mk>
