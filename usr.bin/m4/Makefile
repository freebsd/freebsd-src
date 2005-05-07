#	$OpenBSD: Makefile,v 1.10 2002/04/26 13:13:41 espie Exp $
# $FreeBSD: src/usr.bin/m4/Makefile,v 1.6 2002/05/01 21:37:29 jmallett Exp $

# -DEXTENDED 
# 	if you want the paste & spaste macros.

PROG=	m4
CFLAGS+=-DEXTENDED

SRCS=	eval.c expr.c look.c main.c misc.c gnum4.c trace.c
MAN=	m4.1

.include <bsd.prog.mk>
