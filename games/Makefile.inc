#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93

BINOWN?=	games
.if defined(HIDEGAME)
BINDIR?=	/usr/games/hide
BINMODE?=	4700
.else
BINDIR?=	/usr/games
.endif
