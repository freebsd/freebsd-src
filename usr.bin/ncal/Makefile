# $FreeBSD$

.include <src.opts.mk>

PROG=	ncal

LIBADD=	calendar ncursesw

LINKS=	${BINDIR}/ncal ${BINDIR}/cal
MLINKS=	ncal.1 cal.1

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
