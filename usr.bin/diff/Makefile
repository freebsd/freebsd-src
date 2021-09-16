# $FreeBSD$

.include <src.opts.mk>

PROG=	diff
SRCS=	diff.c diffdir.c diffreg.c xmalloc.c pr.c
LIBADD=	m

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
