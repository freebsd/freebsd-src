#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
# $FreeBSD$

BINGRP=       ${GAMEGRP}
BINDIR?=      /usr/games

.if defined(HIDEGAME)
GBINDIR:=      ${BINDIR}
BINDIR:=       ${BINDIR}/hide
BINMODE=       550
.endif
