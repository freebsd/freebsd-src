#        @(#)Makefile	5.1 (Berkeley) 6/8/92

PROG=	test
SRCS=   test.c operators.c
CFLAGS+=-I.
LINKS=	${BINDIR}/test ${BINDIR}/[
MLINKS=	test.1 '[.1'


# use this rule to if you update binary_ops, or unary_ops
make_op: 
	sh ${.CURDIR}/mkops

.include <bsd.prog.mk>
