#	$OpenBSD: Makefile,v 1.10 2002/04/26 13:13:41 espie Exp $

# -DEXTENDED 
# 	if you want the paste & spaste macros.

PROG=	m4
CFLAGS+=-DEXTENDED
CDIAGFLAGS=-W -Wall -Wstrict-prototypes -pedantic \
	-Wno-unused -Wno-char-subscripts -Wno-sign-compare

SRCS=	eval.c expr.c look.c main.c misc.c gnum4.c trace.c
MAN=	m4.1

.include <bsd.prog.mk>
