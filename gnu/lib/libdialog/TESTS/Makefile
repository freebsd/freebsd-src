# Makefile for dialog
# $Id: Makefile,v 1.4 1994/10/28 03:12:45 ache Exp $

PROG=	test1

CFLAGS+= -Wall -Wstrict-prototypes

DPADD+= $(LIBDIALOG) $(LIBNCURSES) $(LIBMYTINFO)
LDADD+= -ldialog -lncurses -lmytinfo

.include <bsd.prog.mk>
