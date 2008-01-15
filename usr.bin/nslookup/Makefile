# $FreeBSD: src/usr.bin/nslookup/Makefile,v 1.4.2.2 2006/12/19 18:25:29 dougb Exp $

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

MANFILTER=	sed -e "s@^host \[server\]@\\\fBhost\\\fR \\\fI[server]\\\fR@"

.include <bsd.prog.mk>
