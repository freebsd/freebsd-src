#	$Id$

BINOWN?=	games
.if defined(HIDEGAME)
BINDIR?=	/usr/games/hide
BINMODE?=	4700
.else
BINDIR?=	/usr/games
.endif
