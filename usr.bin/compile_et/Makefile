# $FreeBSD: src/usr.bin/compile_et/Makefile,v 1.12.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/com_err

PROG=	compile_et
SRCS=	compile_et.c parse.y lex.l getarg.c
CFLAGS+=-I. -I${.CURDIR}/../../contrib/com_err

.include <bsd.prog.mk>
