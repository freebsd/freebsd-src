# $FreeBSD: src/usr.bin/compile_et/Makefile,v 1.11.34.1.8.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/com_err

PROG=	compile_et
SRCS=	compile_et.c parse.y lex.l getarg.c
CFLAGS+=-I. -I${.CURDIR}/../../contrib/com_err

.include <bsd.prog.mk>
