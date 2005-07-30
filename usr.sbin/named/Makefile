# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../contrib/bind9
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/named

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		named

.PATH: ${SRCDIR}/unix
SRCS+=		os.c

.PATH: ${SRCDIR}
SRCS+=		aclconf.c builtin.c client.c config.c control.c \
		controlconf.c interfacemgr.c \
		listenlist.c log.c logconf.c main.c notify.c \
		query.c server.c sortlist.c \
		tkeyconf.c tsigconf.c update.c xfrout.c \
		zoneconf.c \
		lwaddr.c lwresd.c lwdclient.c lwderror.c lwdgabn.c \
		lwdgnba.c lwdgrbn.c lwdnoop.c lwsearch.c

CFLAGS+=	-I${SRCDIR}/unix/include -I${SRCDIR}/include -I${LIB_BIND_DIR}

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD}

MAN=		named.8 lwresd.8 named.conf.5

MANFILTER=	sed -e "s@/etc/named\.conf@/etc/namedb/named.conf@g"

LINKS=		${BINDIR}/named ${BINDIR}/lwresd

.include <bsd.prog.mk>
