# $FreeBSD: src/usr.sbin/pkg_install/add/Makefile,v 1.17 2003/05/19 15:52:00 des Exp $

PROG=	pkg_add
SRCS=	main.c perform.c futil.c extract.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	2

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
DPADD+=	${LIBSSL} ${LIBCRYPTO}
LDADD+=	-lssl -lcrypto
.endif

.include <bsd.prog.mk>
