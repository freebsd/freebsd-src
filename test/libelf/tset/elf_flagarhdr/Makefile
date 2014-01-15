# $Id: Makefile 2077 2011-10-27 03:59:40Z jkoshy $

TOP=	../../../..

TS_SRCS=		flagarhdr.m4

TS_DATA=		a.ar
CLEANFILES+=		a1

a1:	.SILENT
	echo "a1" > ${.TARGET}

a.ar:	a1 .SILENT
	${AR} crv ${.TARGET} a1 > /dev/null

.include "${TOP}/mk/elftoolchain.tet.mk"
