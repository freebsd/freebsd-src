#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
# $FreeBSD$

BINDIR?=	/usr/games

.if defined(HIDEGAME)
ORIGBINDIR:=	${BINDIR}
BINDIR:=	${BINDIR}/hide
ORIGBINGRP:=	${BINGRP}
BINGRP=		games
BINMODE=	550
.endif
