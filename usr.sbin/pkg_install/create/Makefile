# $FreeBSD: src/usr.sbin/pkg_install/create/Makefile,v 1.18 2003/05/19 15:52:00 des Exp $

PROG=	pkg_create
SRCS=	main.c perform.c pl.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	2

DPADD=	${LIBINSTALL} ${LIBMD}
LDADD=	${LIBINSTALL} -lmd

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
DPADD+=	${LIBSSL} ${LIBCRYPTO}
LDADD+=	-lssl -lcrypto
.endif

.include <bsd.prog.mk>
