# $Id: Makefile,v 1.3.2.1 1999/08/16 06:17:12 obrien Exp $

.PATH:	${.CURDIR}/../../../contrib/bison

SUBDIR=	data doc

PROG=	bison
MAN1=	bison.1

DATADIR= ${SHAREDIR}/misc

CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -DXPFILE=\"${DATADIR}/bison.simple\"
CFLAGS+= -DXPFILE1=\"${DATADIR}/bison.hairy\"
CFLAGS+= -I${.CURDIR}

SRCS+=	LR0.c allocate.c closure.c conflicts.c derives.c files.c \
	getargs.c gram.c lalr.c lex.c main.c nullable.c output.c \
	print.c reader.c reduce.c symtab.c warshall.c getopt.c getopt1.c

.include <bsd.prog.mk>
