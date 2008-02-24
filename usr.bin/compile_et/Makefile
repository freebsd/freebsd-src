# $FreeBSD: src/usr.bin/compile_et/Makefile,v 1.12 2005/10/14 15:50:22 ru Exp $

.PATH: ${.CURDIR}/../../contrib/com_err

PROG=	compile_et
SRCS=	compile_et.c parse.y lex.l getarg.c
CFLAGS+=-I. -I${.CURDIR}/../../contrib/com_err

.include <bsd.prog.mk>
