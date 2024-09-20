.include <src.opts.mk>

PROG=	tail
SRCS=	forward.c misc.c read.c reverse.c tail.c
LIBADD=	util

.if ${MK_CASPER} != "no" && !defined(RESCUE)
LIBADD+= casper
LIBADD+= cap_fileargs
CFLAGS+= -DWITH_CASPER
.endif

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
