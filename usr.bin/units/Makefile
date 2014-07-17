# $FreeBSD$

.include <src.opts.mk>

PROG=	units
FILES=	units.lib
FILESDIR=	${SHAREDIR}/misc

LDADD+=-ledit -ltermcap
DPADD+=${LIBEDIT} ${LIBTERMCAP}

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
