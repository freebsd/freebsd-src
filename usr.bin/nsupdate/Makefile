# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../contrib/bind9
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/nsupdate

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		nsupdate

.PATH: ${SRCDIR}
SRCS+=		nsupdate.c

CFLAGS+=	-I${SRCDIR}/include

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD}

MAN=		nsupdate.8

MANFILTER=	sed -e "s@/etc/named\.conf@/etc/namedb/named.conf@g"

.include <bsd.prog.mk>
