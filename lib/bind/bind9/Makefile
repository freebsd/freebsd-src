# $FreeBSD: src/lib/bind/bind9/Makefile,v 1.4.2.1 2004/09/26 03:10:11 des Exp $

BIND_DIR=	${.CURDIR}/../../../contrib/bind9
LIB_BIND_REL=	..
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/lib/bind9

.include	"${LIB_BIND_DIR}/config.mk"

LIB=		bind9

.PATH:		${SRCDIR}
SRCS=		check.c getaddresses.c version.c

CFLAGS+=	-I${SRCDIR}/include

DPADD=		${PTHREAD_DPADD}
LDADD=		${PTHREAD_LDADD}

.if defined(WITH_BIND_LIBS)
INCS=		${SRCDIR}/include/bind9/check.h \
		${SRCDIR}/include/bind9/getaddresses.h \
		${SRCDIR}/include/bind9/version.h

INCSDIR=	${INCLUDEDIR}/bind9
.endif

.include <bsd.lib.mk>
