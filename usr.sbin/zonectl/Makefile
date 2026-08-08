.include <src.opts.mk>

PROG=	zonectl
SRCS=	zonectl.c
SDIR=	${SRCTOP}/sys
LIBADD= cam sbuf util
MAN=	zonectl.8

HAS_TESTS=
SUBDIR.${MK_TESTS}+=	tests

.include <bsd.prog.mk>
