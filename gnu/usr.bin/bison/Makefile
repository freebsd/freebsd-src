# $Id$

.PATH:	${.CURDIR}/../../../contrib/bison

SUBDIR=	data doc

PROG=	bison
MAN1=	bison.1

DATADIR= ${SHAREDIR}/misc

CFLAGS+= -DSTDC_HEADERS=1 -DHAVE_STRING_H=1 -DHAVE_STDLIB_H=1 -DHAVE_ALLOCA=1 \
	 -DHAVE_STRERROR=1 
CFLAGS+= -DXPFILE=\"${DATADIR}/bison.simple\"
CFLAGS+= -DXPFILE1=\"${DATADIR}/bison.hairy\"

SRCS+=	LR0.c allocate.c closure.c conflicts.c derives.c files.c \
	getargs.c gram.c lalr.c lex.c main.c nullable.c output.c \
	print.c reader.c reduce.c symtab.c warshall.c version.c \
	getopt.c getopt1.c

.include <bsd.prog.mk>
