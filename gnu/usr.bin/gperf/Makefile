# $FreeBSD: src/gnu/usr.bin/gperf/Makefile,v 1.4.2.1 2001/02/15 12:38:20 obrien Exp $

GPROFDIR=	${.CURDIR}/../../../contrib/gperf

.PATH: ${GPROFDIR}/src ${GPROFDIR}/lib ${GPROFDIR}/doc

SUBDIR=	doc

PROG=	gperf
SRCS=	bool-array.cc gen-perf.cc hash-table.cc iterator.cc key-list.cc \
	list-node.cc main.cc new.cc options.cc read-line.cc trace.cc \
	vectors.cc version.cc
SRCS+=	hash.cc getopt.c getopt1.c

CXXFLAGS+=	-I${GPROFDIR}/lib -I${.CURDIR}

.include <bsd.prog.mk>
