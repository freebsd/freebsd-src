# $FreeBSD: src/gnu/usr.bin/gperf/Makefile,v 1.4 1999/08/27 23:35:35 peter Exp $

.PATH:	${.CURDIR}/../../../contrib/gperf
.PATH:	${.CURDIR}/../../../contrib/gperf/src

SUBDIR=	doc

PROG=	gperf
MAN1=	gperf.1

CFLAGS+= -D_HAVE_PARAM_H -DLO_CAL -DGATHER_STATISTICS #-DRLIMIT_STACK 

SRCS =	options.c iterator.c main.c perfect.c keylist.c listnode.c xmalloc.c \
	hashtable.c boolarray.c readline.c stderr.c version.c getopt.c

.include <bsd.prog.mk>
