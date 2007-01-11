# $FreeBSD: src/usr.bin/compile_et/Makefile,v 1.11.34.1 2005/11/16 08:21:06 ru Exp $

.PATH: ${.CURDIR}/../../contrib/com_err

PROG=	compile_et
SRCS=	compile_et.c parse.y lex.l getarg.c
CFLAGS+=-I. -I${.CURDIR}/../../contrib/com_err

.include <bsd.prog.mk>
