# Makefile for dialog
# $Id$

PROG=   dialog
MAN1=   dialog.1
SRCS=	dialog.c rc.c checklist.c inputbox.c menubox.c msgbox.c \
	radiolist.c textbox.c yesno.c

CFLAGS+= -Wall -Wstrict-prototypes -DHAVE_NCURSES -DLOCALE

DPADD+= $(LIBNCURSES) $(LIBMYTINFO)
LDADD+= -lncurses -lmytinfo

.include <bsd.prog.mk>
