# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../contrib/bind9
LIB_BIND_DIR=	${.CURDIR}/../../lib/bind
SRCDIR=		${BIND_DIR}/bin/check

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		named-checkconf

.PATH: ${SRCDIR}
SRCS+=		named-checkconf.c check-tool.c

CFLAGS+=	-I${LIB_BIND_DIR}

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

MAN=		named-checkconf.8

.include <bsd.prog.mk>
