# $FreeBSD$

PROG=	compile_et
SRCS=	compile_et.c parse.y lex.l getarg.c
CFLAGS+=-I. -I${.CURDIR}/../../contrib/com_err

.include <bsd.prog.mk>

.PATH: ${.CURDIR}/../../contrib/com_err
