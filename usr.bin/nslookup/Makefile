# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../contrib/bind9
LIB_BIND_DIR=	${.CURDIR}/../../lib/bind
SRCDIR=		${BIND_DIR}/bin/dig

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		nslookup

.PATH: ${SRCDIR}
SRCS+=		dighost.c nslookup.c

CFLAGS+=	-I${SRCDIR}/include

DPADD+=		${BIND_DPADD}
LDADD+=		${BIND_LDADD}

MAN=		nslookup.1

.include <bsd.prog.mk>
