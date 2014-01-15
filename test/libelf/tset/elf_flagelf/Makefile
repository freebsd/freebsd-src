# $Id: Makefile 1711 2011-08-09 12:53:16Z jkoshy $

TOP=	../../../..

TS_SRCS=		elf.m4
TS_DATA=		a.ar

CLEANFILES=	zero

zero:
	touch ${.TARGET}

a.ar:	zero
	rm -f ${.TARGET}
	ar crv ${.TARGET} ${.ALLSRC}

.include "${TOP}/mk/elftoolchain.tet.mk"
