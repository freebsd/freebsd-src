# Makefile copyright Andreas Klemm <andreas@FreeBSD.ORG> 1998
#
# $Id$

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${.CURDIR}/../../../contrib/bc/dc \
	${.CURDIR}/../../../contrib/bc/lib \
	${.CURDIR}/../../../contrib/bc/doc

PROG=		dc
SRCS=		array.c dc.c eval.c misc.c numeric.c stack.c string.c \
		getopt.c getopt1.c number.c
MAN1=		dc.1

CFLAGS+=	-DHAVE_VPRINTF=1  -DSTDC_HEADERS=1 -DHAVE_ISGRAPH=1 \
		-DHAVE_LIMITS_H=1 -DHAVE_STDARG_H=1 -DHAVE_STDDEF_H=1 \
		-DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_UNISTD_H=1 \
		-I${.CURDIR}/../../../contrib/bc/h -I.

DPADD=		${LIBM}
LDADD=		-lm

SUBDIR+= doc

.include <bsd.prog.mk>
