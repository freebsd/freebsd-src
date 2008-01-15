# $FreeBSD: src/lib/bind/isccfg/Makefile,v 1.4.4.1 2005/11/16 07:59:50 ru Exp $

BIND_DIR=	${.CURDIR}/../../../contrib/bind9
LIB_BIND_REL=	..
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/lib/isccfg

.include	"${LIB_BIND_DIR}/config.mk"

LIB=		isccfg

.PATH:		${SRCDIR}
SRCS=		log.c namedconf.c parser.c version.c

CFLAGS+=	-I${SRCDIR}/include -I${.CURDIR}

.if defined(WITH_BIND_LIBS)
INCS=		${SRCDIR}/include/isccfg/cfg.h \
		${SRCDIR}/include/isccfg/grammar.h \
		${SRCDIR}/include/isccfg/log.h \
		${SRCDIR}/include/isccfg/namedconf.h \
		${SRCDIR}/include/isccfg/version.h

INCSDIR=	${INCLUDEDIR}/isccfg
.endif

.include <bsd.lib.mk>
