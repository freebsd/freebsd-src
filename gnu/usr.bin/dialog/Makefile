# Makefile for dialog
# $FreeBSD$

PROG=   dialog
MAN1=   dialog.1

CFLAGS+= -Wall -Wstrict-prototypes

DPADD+= $(LIBDIALOG) $(LIBNCURSES) $(LIBMYTINFO)
LDADD+= -ldialog -lncurses -lmytinfo

.include <bsd.prog.mk>
