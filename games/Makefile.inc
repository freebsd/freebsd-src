#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93

BINGRP=       games
.if defined(HIDEGAME)
BINDIR=        /usr/games/hide
.if defined(SUIDGAME)
BINOWN=         games
BINMODE=        4550
INSTALLFLAGS+=  -fschg
.else
BINMODE=       550
.endif
.else
BINDIR=        /usr/games
.endif
