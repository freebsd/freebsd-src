# $FreeBSD: src/usr.bin/nslookup/Makefile,v 1.4.2.1 2005/07/30 07:56:27 des Exp $

BIND_DIR=	${.CURDIR}/../../contrib/bind9
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/dig

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		nslookup

.PATH: ${SRCDIR}
SRCS+=		dighost.c nslookup.c

CFLAGS+=	-I${SRCDIR}/include

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD}

.include <bsd.prog.mk>
