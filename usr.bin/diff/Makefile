# $FreeBSD$

.include <src.opts.mk>

PROG=	diff
SRCS=	diff.c diffdir.c diffreg.c xmalloc.c

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
