#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	wall
SRCS=	ttymsg.c wall.c
BINGRP=	tty
BINMODE=2555

DPADD=	${LIBULOG}
LDADD=	-lulog

.include <bsd.prog.mk>
