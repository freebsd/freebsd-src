#	$OpenBSD: Makefile,v 1.10 2002/04/26 13:13:41 espie Exp $
# $FreeBSD: src/usr.bin/m4/Makefile,v 1.7.16.1 2008/10/02 02:57:24 kensmith Exp $

# -DEXTENDED 
# 	if you want the paste & spaste macros.

PROG=	m4
CFLAGS+=-DEXTENDED

SRCS=	eval.c expr.c look.c main.c misc.c gnum4.c trace.c

.include <bsd.prog.mk>
