# $FreeBSD$

PROG=	pkg_create
SRCS=	main.c perform.c pl.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	2

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.if !defined(NOCRYPT) && !defined(NOSECURE) && !defined(NO_OPENSSL)
DPADD+=	${LIBCRYPTO} ${LIBSSL}
LDADD+=	-lcrypto -lssl
.endif

.include <bsd.prog.mk>
