#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	snake
SRCS=	snake.c move.c
MAN6=	snake.6
DPADD=	${LIBM} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-lm -ltermcap -lcompat
HIDEGAME=hidegame
MLINKS=	snake.6 snscore.6

.include "../../Makefile.inc"
.include <bsd.prog.mk>
