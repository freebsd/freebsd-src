# $FreeBSD: src/usr.bin/compile_et/Makefile,v 1.12.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/com_err

PROG=	compile_et
SRCS=	compile_et.c parse.y lex.l getarg.c
CFLAGS+=-I. -I${.CURDIR}/../../contrib/com_err

.include <bsd.prog.mk>
