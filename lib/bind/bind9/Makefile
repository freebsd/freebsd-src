# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../../contrib/bind9
LIB_BIND_DIR=	${.CURDIR}/..
SRCDIR=		${BIND_DIR}/lib/bind9

.include	"${LIB_BIND_DIR}/config.mk"

LIB=		bind9

.PATH:		${SRCDIR}
SRCS=		check.c getaddresses.c version.c

CFLAGS+=	-I${SRCDIR}/include -I${.CURDIR} -I${.CURDIR}/..

DPADD=		${PTHREAD_DPADD}
LDADD=		${PTHREAD_LDADD}

INCS=		${SRCDIR}/include/bind9/check.h \
		${SRCDIR}/include/bind9/getaddresses.h \
		${SRCDIR}/include/bind9/version.h

INCSDIR=	${INCLUDEDIR}/bind9

.include <bsd.lib.mk>
