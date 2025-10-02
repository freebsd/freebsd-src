.include <src.opts.mk>

PACKAGE=	sound

PROG=		mixer
SRCS=		${PROG}.c
MAN=		${PROG}.8
LDFLAGS+=	-lmixer

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
