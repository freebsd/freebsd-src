#	$OpenBSD: Makefile,v 1.8 2001/09/18 14:55:52 espie Exp $
# $FreeBSD$

# -DEXTENDED 
# 	if you want the paste & spaste macros.

PROG=	m4
CFLAGS+=-DEXTENDED

SRCS=	eval.c expr.c look.c main.c misc.c gnum4.c trace.c
WARNS?=	2
MAN=	m4.1

.include <bsd.prog.mk>
