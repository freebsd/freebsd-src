# Makefile for dialog
# $FreeBSD: src/gnu/usr.bin/dialog/Makefile,v 1.8 2000/01/07 13:34:56 phantom Exp $

SUBDIR=	TESTS

PROG=   dialog
MAN1=   dialog.1

CFLAGS+= -Wall -Wstrict-prototypes

DPADD+= $(LIBDIALOG) $(LIBNCURSES) $(LIBMYTINFO)
LDADD+= -ldialog -lncurses -lmytinfo

.include <bsd.prog.mk>
