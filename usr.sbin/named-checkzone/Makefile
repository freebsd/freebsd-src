# $FreeBSD: src/usr.sbin/named-checkzone/Makefile,v 1.5.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

BIND_DIR=	${.CURDIR}/../../contrib/bind9
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/check

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		named-checkzone

.PATH: ${SRCDIR}
SRCS+=		named-checkzone.c check-tool.c

CFLAGS+=	-I${LIB_BIND_DIR}

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

LINKS=		${BINDIR}/named-checkzone ${BINDIR}/named-compilezone

MAN=		named-checkzone.8

MLINKS=		named-checkzone.8 named-compilezone.8

.include <bsd.prog.mk>
