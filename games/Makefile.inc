#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93

.if defined(HIDEGAME)
BINOWN=        games
BINDIR=        /usr/games/hide
BINMODE=       4500
.else
BINDIR=        /usr/games
.endif
