# Makefile for dialog
# $Id: Makefile,v 1.3 1994/10/11 23:51:06 ache Exp $

PROG=   dialog
MAN1=   dialog.1

CFLAGS+= -Wall -Wstrict-prototypes

DPADD+= $(LIBDIALOG) $(LIBNCURSES) $(LIBMYTINFO)
LDADD+= -ldialog -lncurses -lmytinfo

.include <bsd.prog.mk>
