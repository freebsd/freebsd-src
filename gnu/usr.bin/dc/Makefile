#	$Id$

.PATH:	${.CURDIR}/../../../contrib/bc/dc \
	${.CURDIR}/../../../contrib/bc/lib \
	${.CURDIR}/../../../contrib/bc/doc

PROG=	dc
SRCS=	array.c dc.c eval.c misc.c numeric.c stack.c string.c \
	getopt.c getopt1.c number.c
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../../contrib/bc/h
DPADD=	${LIBM}
LDADD=	-lm
SUBDIR=	doc

.include <bsd.prog.mk>
