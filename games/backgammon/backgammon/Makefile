#	@(#)Makefile	5.16 (Berkeley) 5/11/90

PROG=	backgammon
CFLAGS+=-DV7 -I${.CURDIR}/../common_source
SRCS=	allow.c board.c check.c extra.c fancy.c init.c main.c move.c \
	odds.c one.c save.c subs.c table.c text.c version.c
MAN6=	backgammon.6
DPADD=	${LIBTERM}
LDADD=	-ltermcap
.PATH:	${.CURDIR}/../common_source
HIDEGAME=hidegame

.include "../../Makefile.inc"
.include <bsd.prog.mk>
