# $FreeBSD$

PROG=	units
FILES=	units.lib
FILESDIR=	${SHAREDIR}/misc

LDADD+=-ledit -ltermcap
DPADD+=${LIBEDIT} ${LIBTERMCAP}

.include <bsd.prog.mk>
