#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	hangman
SRCS=	endgame.c extern.c getguess.c getword.c main.c playgame.c \
	prdata.c prman.c prword.c setup.c
MAN6=	hangman.6
DPADD=	${LIBCURSES} ${LIBTERM} ${LIBCOMPAT}
LDADD=	-lcurses -ltermlib -lcompat
HIDEGAME=hidegame

.include <bsd.prog.mk>


