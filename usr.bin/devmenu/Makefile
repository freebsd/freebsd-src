#	Makefile,v 1.2 1995/04/14 16:13:28 ache Exp

PROG=	devmenu
SRCS=	devmenu.c devfilter.c ifmenu.c
MAN1=	devmenu.1
LDADD+= -ldialog -lncurses -lmytinfo
DPADD+= ${LIBDIALOG} ${LIBNCURSES} ${LIBMYTINFO}
CFLAGS+= -I/sys

.include <bsd.prog.mk>
