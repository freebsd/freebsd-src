# Makefile for dialog
# $Id: Makefile,v 1.2 1994/10/09 00:46:16 pst Exp $

PROG=   dialog
MAN1=   dialog.1

CFLAGS+= -Wall -Wstrict-prototypes -DHAVE_NCURSES

DPADD+= $(LIBDIALOG) $(LIBNCURSES) $(LIBMYTINFO)
LDADD+= -ldialog -lncurses -lmytinfo

.include <bsd.prog.mk>
