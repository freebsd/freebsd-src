# Makefile for dialog
# $FreeBSD$

.include <bsd.own.mk>

.if ${MK_EXAMPLES} != "no"
SUBDIR=	TESTS
.endif

PROG=   dialog

CFLAGS+= -Wall -Wstrict-prototypes

DPADD=	$(LIBDIALOG) $(LIBNCURSES)
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
