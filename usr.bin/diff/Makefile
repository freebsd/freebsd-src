.include <src.opts.mk>

PROG=	diff
SRCS=	diff.c diffdir.c diffreg.c xmalloc.c pr.c diffreg_new.c

LIBADD=	m diff
CFLAGS+= -I${.CURDIR} -I${SRCTOP}/contrib/libdiff/lib -I${SRCTOP}/contrib/libdiff/include

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
