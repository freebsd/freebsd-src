#	@(#)Makefile.inc	5.1 (Berkeley) 5/11/90

BINOWN?=	games
.if defined(HIDEGAME)
BINDIR?=	/usr/games/hide
BINMODE?=	4700
.else
BINDIR?=	/usr/games
.endif
