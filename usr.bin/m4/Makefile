#	$OpenBSD: Makefile,v 1.10 2002/04/26 13:13:41 espie Exp $
# $FreeBSD: src/usr.bin/m4/Makefile,v 1.2.12.1 2002/07/15 02:06:15 jmallett Exp $

# -DEXTENDED 
# 	if you want the paste & spaste macros.

PROG=	m4
CFLAGS+=-DEXTENDED

SRCS=	eval.c expr.c look.c main.c misc.c gnum4.c trace.c
MAN=	m4.1

.include <bsd.prog.mk>
