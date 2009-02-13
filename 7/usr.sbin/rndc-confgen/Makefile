# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../contrib/bind9
LIB_BIND_REL=	../../lib/bind
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/bin/rndc

.include	"${LIB_BIND_DIR}/config.mk"

PROG=		rndc-confgen

.PATH: ${SRCDIR}/unix
SRCS+=		os.c

.PATH: ${SRCDIR}
SRCS+=		rndc-confgen.c util.c

CFLAGS+=	-I${SRCDIR}/unix/include -I${SRCDIR}/include -I${LIB_BIND_DIR}

DPADD+=		${BIND_DPADD} ${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD+=		${BIND_LDADD} ${CRYPTO_LDADD} ${PTHREAD_LDADD}

MAN=		rndc-confgen.8

MANFILTER=	sed -e 's@fI/etc\\fR.*@fI/etc/namedb\\fR@' \
		-e '/^sysconfdir$$/d' \
		-e '/was specified as when BIND was built)/d'

.include <bsd.prog.mk>
