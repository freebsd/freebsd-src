#	@(#)Makefile	5.10 (Berkeley) 5/11/90

PROG=	mille
SRCS=	comp.c end.c extern.c init.c mille.c misc.c move.c print.c \
	roll.c save.c types.c varpush.c
DPADD=	${LIBCURSES} ${LIBTERM}
LDADD=	-lcurses -ltermlib
MAN6=	mille.6
HIDEGAME=hidegame

.include <bsd.prog.mk>

