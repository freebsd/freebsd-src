#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/backgammon/backgammon/Makefile,v 1.2.14.1 2001/04/25 09:28:44 ru Exp $

PROG=	backgammon
CFLAGS+=-DV7 -I${.CURDIR}/../common_source
SRCS=	allow.c board.c check.c extra.c fancy.c init.c main.c move.c \
	odds.c one.c save.c subs.c table.c text.c version.c
MAN=	backgammon.6
DPADD=	${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-ltermcap -lcompat
.PATH:	${.CURDIR}/../common_source
HIDEGAME=hidegame

.include "../../Makefile.inc"
.include <bsd.prog.mk>
