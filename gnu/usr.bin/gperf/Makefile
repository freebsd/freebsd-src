# $FreeBSD$

.PATH:	${.CURDIR}/../../../contrib/gperf
.PATH:	${.CURDIR}/../../../contrib/gperf/src

SUBDIR=	doc

PROG=	gperf
MAN1=	gperf.1

CFLAGS+= -D_HAVE_PARAM_H -DLO_CAL -DGATHER_STATISTICS #-DRLIMIT_STACK 

SRCS =	options.c iterator.c main.c perfect.c keylist.c listnode.c xmalloc.c \
	hashtable.c boolarray.c readline.c stderr.c version.c getopt.c

.include <bsd.prog.mk>
