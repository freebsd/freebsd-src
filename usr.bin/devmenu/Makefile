#	$Id: Makefile,v 1.1 1995/04/13 21:10:56 wollman Exp $

PROG=	devmenu
SRCS=	devmenu.c devfilter.c ifmenu.c
MAN1=	devmenu.1
LDADD+= -ldialog -lncurses -lmytinfo
DPADD+= ${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}
CFLAGS+= -I/sys

.include <bsd.prog.mk>
