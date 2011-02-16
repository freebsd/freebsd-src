# $FreeBSD: src/gnu/usr.bin/gperf/Makefile,v 1.11.34.1 2010/12/21 17:10:29 kensmith Exp $

GPROFDIR=	${.CURDIR}/../../../contrib/gperf

.PATH: ${GPROFDIR}/src ${GPROFDIR}/lib ${GPROFDIR}/doc

SUBDIR=	doc

PROG_CXX=	gperf
SRCS=	bool-array.cc gen-perf.cc hash-table.cc iterator.cc key-list.cc \
	list-node.cc main.cc new.cc options.cc read-line.cc trace.cc \
	vectors.cc version.cc
SRCS+=  hash.cc

CXXFLAGS+=	-I${GPROFDIR}/lib -I${.CURDIR}

.include <bsd.prog.mk>
