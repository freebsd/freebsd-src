# Makefile for dialog
# $FreeBSD: src/gnu/usr.bin/dialog/Makefile,v 1.10 2001/03/27 14:58:41 ru Exp $

SUBDIR=	TESTS

PROG=   dialog

CFLAGS+= -Wall -Wstrict-prototypes

DPADD+= $(LIBDIALOG) $(LIBNCURSES)
LDADD+= -ldialog -lncurses

.include <bsd.prog.mk>
