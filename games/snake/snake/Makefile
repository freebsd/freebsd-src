#	@(#)Makefile	5.11 (Berkeley) 2/28/91

PROG=	snake
SRCS=	snake.c move.c
MAN6=	snake.6
DPADD=	${LIBM} ${LIBTERM}
LDADD=	-lm -ltermcap
HIDEGAME=hidegame

.include "../../Makefile.inc"
.include <bsd.prog.mk>
