# Makefile for dialog
# $FreeBSD: src/gnu/usr.bin/dialog/Makefile,v 1.13.8.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

.if ${MK_EXAMPLES} != "no"
SUBDIR=	TESTS
.endif

PROG=   dialog

CFLAGS+= -Wall -Wstrict-prototypes

DPADD=	$(LIBDIALOG) $(LIBNCURSES)
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
