# $FreeBSD: src/gnu/usr.bin/gperf/Makefile,v 1.11.30.2.2.1 2012/03/03 06:15:13 kensmith Exp $

.include <bsd.own.mk>
GPERFDIR=	${.CURDIR}/../../../contrib/gperf

.PATH: ${GPERFDIR}/src ${GPERFDIR}/lib ${GPERFDIR}/doc

SUBDIR=	doc

PROG_CXX=	gperf
SRCS=	bool-array.cc hash-table.cc input.cc keyword-list.cc keyword.cc \
	main.cc options.cc output.cc positions.cc search.cc version.cc \
	getline.cc hash.cc
WARNS=	1

CXXFLAGS+=	-I${GPERFDIR}/lib -I${.CURDIR}

.include <bsd.prog.mk>
