#	$Id: Makefile.inc,v 1.1 1994/01/30 00:55:54 rgrimes Exp $

BINOWN?=	games
.if defined(HIDEGAME)
BINDIR?=	/usr/games/hide
BINMODE?=	4700
.else
BINDIR?=	/usr/games
.endif
