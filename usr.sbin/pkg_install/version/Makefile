# $FreeBSD: src/usr.sbin/pkg_install/version/Makefile,v 1.11 2002/06/24 16:03:24 markm Exp $

PROG=	pkg_version
SRCS=	main.c perform.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	2

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.if !defined(NOCRYPT) && !defined(NOSECURE) && !defined(NO_OPENSSL)
DPADD+=	${LIBSSL} ${LIBCRYPTO}
LDADD+=	-lssl -lcrypto
.endif

test:
	./test-pkg_version.sh

.include <bsd.prog.mk>
