#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/hangman/Makefile,v 1.2.14.1 2001/04/25 09:28:57 ru Exp $

PROG=	hangman
SRCS=	endgame.c extern.c getguess.c getword.c main.c playgame.c \
	prdata.c prman.c prword.c setup.c
MAN=	hangman.6
DPADD=	${LIBCURSES} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-lcurses -ltermcap -lcompat
HIDEGAME=hidegame

.include <bsd.prog.mk>


