# Makefile for dialog
# $FreeBSD: src/gnu/usr.bin/dialog/Makefile,v 1.13.14.1 2010/12/21 17:10:29 kensmith Exp $

.include <bsd.own.mk>

.if ${MK_EXAMPLES} != "no"
SUBDIR=	TESTS
.endif

PROG=   dialog

CFLAGS+= -Wall -Wstrict-prototypes

DPADD=	$(LIBDIALOG) $(LIBNCURSES)
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
