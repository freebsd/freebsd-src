.include <src.opts.mk>

PROG=		mixer
SRCS=		${PROG}.c
MAN=		${PROG}.8
LDFLAGS+=	-lmixer

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
