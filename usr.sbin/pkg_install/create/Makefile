# $FreeBSD: src/usr.sbin/pkg_install/create/Makefile,v 1.17 2002/11/06 08:57:03 obrien Exp $

PROG=	pkg_create
SRCS=	main.c perform.c pl.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	2

DPADD=	${LIBINSTALL} ${LIBMD}
LDADD=	${LIBINSTALL} -lmd

.if !defined(NOCRYPT) && !defined(NOSECURE) && !defined(NO_OPENSSL)
DPADD+=	${LIBSSL} ${LIBCRYPTO}
LDADD+=	-lssl -lcrypto
.endif

.include <bsd.prog.mk>
