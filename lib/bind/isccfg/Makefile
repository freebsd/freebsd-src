# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../../contrib/bind9
LIB_BIND_DIR=	${.CURDIR}/..
SRCDIR=		${BIND_DIR}/lib/isccfg

.include	"${LIB_BIND_DIR}/config.mk"

LIB=		isccfg

.PATH:		${SRCDIR}
SRCS=		log.c namedconf.c parser.c version.c

CFLAGS+=	-I${SRCDIR}/include -I${.CURDIR} -I${.CURDIR}/..

DPADD=		${LIBPTHREAD}
LDADD=		-lpthread

INCS=		${SRCDIR}/include/isccfg/cfg.h \
		${SRCDIR}/include/isccfg/grammar.h \
		${SRCDIR}/include/isccfg/log.h \
		${SRCDIR}/include/isccfg/namedconf.h \
		${SRCDIR}/include/isccfg/version.h

INCSDIR=	${INCLUDEDIR}/isccfg

.include <bsd.lib.mk>
