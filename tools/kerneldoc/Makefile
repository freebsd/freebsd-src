# $FreeBSD: src/tools/kerneldoc/Makefile,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:	${.CURDIR}/../kern

SRCS+=		kern
SRCS+=		sys
MFILES+=	device_if.m
MFILES+=	bus_if.m

all:	${MFILES:T:S/.m/.h/} ${SRCS}
	doxygen

LN?=		ln
AWK?=		awk
S?=		${.CURDIR}/..

CLEANFILES+=	${SRCS}
.for dir in ${SRCS}
${dir}:
	${LN} -sf ${.CURDIR}/../${dir} .
.endfor

.for mfile in ${MFILES}
CLEANFILES+=	${mfile:T:S/.m$/.h/}
${mfile:T:S/.m$/.h/}: ${mfile}
	${AWK} -f $S/tools/makeobjops.awk $> -h
.endfor

clean::
	rm -f ${CLEANFILES}
