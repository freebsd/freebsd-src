# $FreeBSD$

GPROFDIR=	${.CURDIR}/../../../contrib/gperf

SUBDIR=	doc

PROG=	gperf
MAN1=	gperf.1

CXXFLAGS+=	-I${GPROFDIR}/lib -I${.CURDIR}

SRCS=	bool-array.cc gen-perf.cc hash-table.cc iterator.cc key-list.cc \
	list-node.cc main.cc new.cc options.cc read-line.cc trace.cc \
	vectors.cc version.cc
SRCS+=	hash.cc getopt.c getopt1.c

.include <bsd.prog.mk>
.PATH:	${GPROFDIR}/src
.PATH:	${GPROFDIR}/lib
.PATH:	${GPROFDIR}/doc
