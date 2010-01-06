# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../contrib/bind9
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/named

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		named

CONFIGARGS='--prefix=/usr' '--infodir=/usr/share/info' '--mandir=/usr/share/man' '--enable-threads' '--disable-ipv6' '--enable-getifaddrs' '--disable-linux-caps' '--with-openssl=/usr' '--with-randomdev=/dev/random'

# Optional features
.if ${MK_BIND_LARGE_FILE} == "yes"
CONFIGARGS+='--enable-largefile'
.endif
.if ${MK_BIND_SIGCHASE} == "yes"
CONFIGARGS+='STD_CDEFINES=-DDIG_SIGCHASE=1'
.endif
.if ${MK_BIND_IDN} == "yes"
CONFIGARGS+='--with-idn=/usr/local'
.else
CONFIGARGS+='--without-idn'
.endif
.if ${MK_BIND_XML} == "yes"
CONFIGARGS+='--with-libxml2=/usr/local'
.else
CONFIGARGS+='--without-libxml2'
.endif

.PATH: ${SRCDIR}/unix
SRCS+=		os.c

.PATH: ${SRCDIR}
SRCS+=		builtin.c client.c config.c control.c \
		controlconf.c interfacemgr.c \
		listenlist.c log.c logconf.c main.c notify.c \
		query.c server.c sortlist.c \
		tkeyconf.c tsigconf.c update.c xfrout.c \
		zoneconf.c \
		lwaddr.c lwresd.c lwdclient.c lwderror.c lwdgabn.c \
		lwdgnba.c lwdgrbn.c lwdnoop.c lwsearch.c \
		statschannel.c

CFLAGS+=	-DCONFIGARGS="\"${CONFIGARGS}\""

CFLAGS+=	-I${SRCDIR}/unix/include -I${SRCDIR}/include -I${LIB_BIND_DIR}
CFLAGS+=	-I${BIND_DIR}/lib/isc/${ISC_ATOMIC_ARCH}/include

# Remove the date stamp to make it more obvious when real changes happen
CFLAGS+=	-U__DATE__

WARNS?=		0

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

MAN=		named.8 lwresd.8 named.conf.5

MANFILTER=	sed -e "s@/etc/named\.conf@/etc/namedb/named.conf@g"

LINKS=		${BINDIR}/named ${BINDIR}/lwresd

.include <bsd.prog.mk>
