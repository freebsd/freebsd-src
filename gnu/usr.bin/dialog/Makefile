# Makefile for dialog
# $FreeBSD: src/gnu/usr.bin/dialog/Makefile,v 1.8.2.1 2001/04/25 12:03:34 ru Exp $

SUBDIR=	TESTS

PROG=   dialog

CFLAGS+= -Wall -Wstrict-prototypes

DPADD+= $(LIBDIALOG) $(LIBNCURSES) $(LIBMYTINFO)
LDADD+= -ldialog -lncurses -lmytinfo

.include <bsd.prog.mk>
