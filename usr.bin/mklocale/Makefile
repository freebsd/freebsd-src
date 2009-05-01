#	@(#)Makefile	8.1 (Berkeley) 6/7/93
# $FreeBSD: src/usr.bin/mklocale/Makefile,v 1.29.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	mklocale
WARNS?=	6
SRCS=	yacc.y lex.l y.tab.h
CFLAGS+= -I. -I${.CURDIR} -I${.CURDIR}/../../lib/libc/locale

.include <bsd.prog.mk>
