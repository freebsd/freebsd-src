# $Id$

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${.CURDIR}/../../../contrib/bc/bc \
	${.CURDIR}/../../../contrib/bc/lib \
	${.CURDIR}/../../../contrib/bc/doc

PROG=		bc
BCSRCS=		execute.c global.c load.c main.c storage.c util.c \
		number.c getopt.c getopt1.c
GENSRCS=	bc.c scan.c
SRCS=		${GENSRCS} ${BCSRCS}
MAN1=		bc.1

YACC=		bison -y

CFLAGS+=	-DHAVE_VPRINTF=1  -DSTDC_HEADERS=1 -DHAVE_ISGRAPH=1 \
		-DHAVE_LIMITS_H=1 -DHAVE_STDARG_H=1 -DHAVE_STDDEF_H=1 \
		-DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_UNISTD_H=1 \
		-I${.CURDIR}/../../../contrib/bc/h -I.

CLEANFILES+=	${GENSRCS} y.tab.h

.include <bsd.prog.mk>
