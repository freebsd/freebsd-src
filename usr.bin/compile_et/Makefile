# $FreeBSD: src/usr.bin/compile_et/Makefile,v 1.12.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/com_err

PROG=	compile_et
SRCS=	compile_et.c parse.y lex.l getarg.c
CFLAGS+=-I. -I${.CURDIR}/../../contrib/com_err

.include <bsd.prog.mk>
