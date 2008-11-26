# $FreeBSD: src/lib/bind/bind9/Makefile,v 1.4.4.1.8.1 2008/10/02 02:57:24 kensmith Exp $

BIND_DIR=	${.CURDIR}/../../../contrib/bind9
LIB_BIND_REL=	..
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/lib/bind9

.include	"${LIB_BIND_DIR}/config.mk"

LIB=		bind9

.PATH:		${SRCDIR}
SRCS=		check.c getaddresses.c version.c

CFLAGS+=	-I${SRCDIR}/include

.if defined(WITH_BIND_LIBS)
INCS=		${SRCDIR}/include/bind9/check.h \
		${SRCDIR}/include/bind9/getaddresses.h \
		${SRCDIR}/include/bind9/version.h

INCSDIR=	${INCLUDEDIR}/bind9
.endif

.include <bsd.lib.mk>
