# $FreeBSD$

.include <src.opts.mk>

PROG=	units
FILES=	units.lib
FILESDIR=	${SHAREDIR}/misc

LDADD+=-ledit -ltermcapw
DPADD+=${LIBEDIT} ${LIBTERMCAPW}

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
