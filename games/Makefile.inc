#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
# $FreeBSD$

BINDIR?=	/usr/games

.if defined(HIDEGAME)
GBINDIR:=	${BINDIR}
BINDIR:=	${BINDIR}/hide
GBINGRP:=	${BINGRP}
BINGRP=		games
BINMODE=	550
.endif
