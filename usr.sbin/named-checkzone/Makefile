# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../contrib/bind9
LIB_BIND_DIR=	${.CURDIR}/../../lib/bind
SRCDIR=		${BIND_DIR}/bin/check

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		named-checkzone

.PATH: ${SRCDIR}
SRCS+=		named-checkzone.c check-tool.c

CFLAGS+=	-I${LIB_BIND_DIR}

DPADD+=		${BIND_DPADD}
LDADD+=		${BIND_LDADD}

MAN=		named-checkzone.8

.include <bsd.prog.mk>
