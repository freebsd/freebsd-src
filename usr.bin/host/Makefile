# $FreeBSD$

.include <bsd.own.mk>

.if ${MK_LDNS_UTILS} != "no"

LDNSDIR=	${.CURDIR}/../../contrib/ldns
LDNSHOSTDIR=	${.CURDIR}/../../contrib/ldns-host

.PATH:		${LDNSHOSTDIR}

PROG=		host
SRCS=		ldns-host.c
MAN=		host.1

host.1: ldns-host.1
	sed -e 's/ldns-//gI' <${.ALLSRC} >${.TARGET} || \
		(rm -rf ${.TARGET} ; false)

CFLAGS+=	-I${LDNSDIR}
DPADD+=		${LIBLDNS} ${LIBCRYPTO}
LDADD+=		-lldns -lcrypto
USEPRIVATELIB=	ldns

.else

BIND_DIR=	${.CURDIR}/../../contrib/bind9
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/dig

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		host

.PATH: ${SRCDIR}
SRCS+=		dighost.c host.c

CFLAGS+=	-I${SRCDIR}/include
CFLAGS+=	-I${BIND_DIR}/lib/isc/${ISC_ATOMIC_ARCH}/include

WARNS?=		0

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

.endif

.include <bsd.prog.mk>
