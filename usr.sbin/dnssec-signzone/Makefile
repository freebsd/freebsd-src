# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../contrib/bind9
LIB_BIND_DIR=	${.CURDIR}/../../lib/bind
SRCDIR=		${BIND_DIR}/bin/dnssec

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		dnssec-signzone

.PATH: ${SRCDIR}
SRCS+=		dnssec-signzone.c dnssectool.c

CFLAGS+=	-I${SRCDIR}/unix/include -I${SRCDIR}/include

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

MAN=		dnssec-signzone.8

.include <bsd.prog.mk>
