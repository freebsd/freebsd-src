#	$Id: Makefile,v 1.10 1998/05/01 19:20:40 ache Exp $

BCDIR=	${.CURDIR}/../../../contrib/bc

.PATH:	${BCDIR}/dc ${BCDIR}/lib ${BCDIR}/doc

PROG=	dc
SRCS=	array.c dc.c eval.c misc.c numeric.c stack.c string.c \
	getopt.c getopt1.c number.c
CFLAGS+=-I${.CURDIR}/../bc -I${BCDIR}/h -DHAVE_CONFIG_H
DPADD=	${LIBM}
LDADD=	-lm
SUBDIR=	doc

.include <bsd.prog.mk>
