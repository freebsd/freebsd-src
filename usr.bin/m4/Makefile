#	$OpenBSD: Makefile,v 1.13 2014/05/12 19:11:19 espie Exp $
# $FreeBSD$

# -DEXTENDED 
# 	if you want the paste & spaste macros.

.include <src.opts.mk>

PROG=	m4
CFLAGS+=-DEXTENDED -I${.CURDIR} -I${.CURDIR}/lib
DPADD=	${LIBY} ${LIBL} ${LIBM}
LDADD=	-ly -ll -lm

NO_WMISSING_VARIABLE_DECLARATIONS=

SRCS=	eval.c expr.c look.c main.c misc.c gnum4.c trace.c parser.y tokenizer.l
.PATH: ${.CURDIR}/lib
SRCS+=	ohash.c
WARNS=	3

tokenizer.o: parser.h

CLEANFILES+=	parser.c parser.h tokenizer.o

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
