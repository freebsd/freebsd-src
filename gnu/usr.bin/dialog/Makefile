# Makefile for dialog
# $FreeBSD: src/gnu/usr.bin/dialog/Makefile,v 1.13.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include <bsd.own.mk>

.if ${MK_EXAMPLES} != "no"
SUBDIR=	TESTS
.endif

PROG=   dialog

CFLAGS+= -Wall -Wstrict-prototypes

DPADD=	$(LIBDIALOG) $(LIBNCURSES)
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
