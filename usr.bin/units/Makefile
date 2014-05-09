# $FreeBSD$

PROG=	units
FILES=	units.lib
FILESDIR=	${SHAREDIR}/misc

LDADD+=-ledit -ltermcapw
DPADD+=${LIBEDIT} ${LIBTERMCAPW}

.include <bsd.prog.mk>
