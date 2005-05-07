# Makefile for dialog
# $FreeBSD: src/gnu/usr.bin/dialog/Makefile,v 1.11 2004/02/05 22:44:22 ru Exp $

SUBDIR=	TESTS

PROG=   dialog

CFLAGS+= -Wall -Wstrict-prototypes

DPADD=	$(LIBDIALOG) $(LIBNCURSES)
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
