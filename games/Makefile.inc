#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93

BINGRP=       games
.if defined(HIDEGAME)
BINDIR?=       /usr/games/hide
BINMODE=       550
.else
BINDIR=        /usr/games
.endif
