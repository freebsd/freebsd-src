# $FreeBSD: src/usr.sbin/dnssec-keygen/Makefile,v 1.2.4.1 2005/07/30 07:56:27 des Exp $

BIND_DIR=	${.CURDIR}/../../contrib/bind9
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/dnssec

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		dnssec-keygen

.PATH: ${SRCDIR}
SRCS+=		dnssec-keygen.c dnssectool.c

CFLAGS+=	-I${SRCDIR}/unix/include -I${SRCDIR}/include

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD}

MAN=		dnssec-keygen.8

.include <bsd.prog.mk>
