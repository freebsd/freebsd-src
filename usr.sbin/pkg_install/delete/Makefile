# $FreeBSD$

PROG=	pkg_delete
SRCS=	main.c perform.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	4

DPADD=	${LIBINSTALL} ${LIBMD}
LDADD=	${LIBINSTALL} -lmd

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
DPADD+=	${LIBSSL} ${LIBCRYPTO}
LDADD+=	-lssl -lcrypto
.endif

.include <bsd.prog.mk>
