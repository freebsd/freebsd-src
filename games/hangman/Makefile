#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

PROG=	hangman
SRCS=	endgame.c extern.c getguess.c getword.c main.c playgame.c \
	prdata.c prman.c prword.c setup.c
MAN=	hangman.6
DPADD=	${LIBCURSES} ${LIBCOMPAT}
LDADD=	-lcurses -lcompat
HIDEGAME=hidegame

.include <bsd.prog.mk>


