# Makefile for dialog
# $FreeBSD$

SUBDIR=	TESTS

PROG=   dialog
MAN1=   dialog.1

CFLAGS+= -Wall -Wstrict-prototypes

DPADD+= $(LIBDIALOG) $(LIBNCURSES)
LDADD+= -ldialog -lncurses

.include <bsd.prog.mk>
