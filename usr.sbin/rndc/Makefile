# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../contrib/bind9
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/rndc

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		rndc

.PATH: ${SRCDIR}
SRCS+=		rndc.c util.c

CFLAGS+=	-I${SRCDIR}/include -I${LIB_BIND_DIR}
CFLAGS+=	-I${BIND_DIR}/lib/isc/${ISC_ATOMIC_ARCH}/include

WARNS?=		3

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

MAN=		rndc.8 rndc.conf.5

MANFILTER=	sed -e "s@/etc/rndc\.conf@/etc/namedb/rndc.conf@g" \
		-e "s@/etc/rndc\.key@/etc/namedb/rndc.key@g"

.include <bsd.prog.mk>
