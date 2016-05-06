# $FreeBSD$

.include <src.opts.mk>

PROG=	units
FILES=	definitions.units
FILESDIR=	${SHAREDIR}/misc

LIBADD=	edit

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
