#	@(#)Makefile	8.1 (Berkeley) 6/7/93
# $FreeBSD$

PROG=	mklocale
SRCS=	yacc.y lex.l y.tab.h
CFLAGS+= -I. -I${.CURDIR} -I${.CURDIR}/../../lib/libc/locale

.include <bsd.prog.mk>
