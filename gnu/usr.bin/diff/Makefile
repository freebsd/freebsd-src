# $FreeBSD: src/gnu/usr.bin/diff/Makefile,v 1.9.2.1 1999/08/29 14:33:00 peter Exp $

.PATH:  ${.CURDIR}/../../../contrib/diff

PROG=	diff
SRCS=	diff.c analyze.c io.c context.c ed.c normal.c ifdef.c util.c dir.c \
	version.c getopt.c getopt1.c side.c cmpbuf.c

# Important for ctype macros!
CFLAGS+=-funsigned-char

CFLAGS+=-I${.CURDIR}/../../../contrib/diff
CFLAGS+=-DHAVE_CONFIG_H
CFLAGS+=-DPR_PROGRAM=\"/usr/bin/pr\"

SUBDIR+= doc

DPADD+= ${LIBGNUREGEX}
LDADD+= -lgnuregex

.include <bsd.prog.mk>
