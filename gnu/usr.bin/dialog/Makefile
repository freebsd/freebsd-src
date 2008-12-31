# Makefile for dialog
# $FreeBSD: src/gnu/usr.bin/dialog/Makefile,v 1.13.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

.if ${MK_EXAMPLES} != "no"
SUBDIR=	TESTS
.endif

PROG=   dialog

CFLAGS+= -Wall -Wstrict-prototypes

DPADD=	$(LIBDIALOG) $(LIBNCURSES)
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
