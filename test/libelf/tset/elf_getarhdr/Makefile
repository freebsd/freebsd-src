# $Id: Makefile 1329 2010-12-26 19:23:13Z jkoshy $

TOP=	../../../..

TS_SRCS=		getarhdr.m4

# These names must match those in the test case code.
TS_DATA=		a.ar a-bsd.ar a1.o a2.o
TS_LONGNAME=		"s------------------------2"
CLEANFILES+=		a1.c a2.c s1 ${TS_LONGNAME} "s 3"

a1.c:	.SILENT
	echo "int a1;" > ${.TARGET}
a2.c:	.SILENT
	echo "int a2;" > ${.TARGET}

a.ar:	a1.o a2.o .SILENT
	rm -f ${.TARGET}
	echo 'This is s1.' > s1
	echo 's2.' > ${TS_LONGNAME}
	echo 's-3.' > "s 3"
	${AR} crv ${.TARGET} s1 a1.o ${TS_LONGNAME} a2.o "s 3" > /dev/null

a-bsd.ar: a.ar .SILENT
	rm -f ${.TARGET}
	${ELFTOOLCHAIN_AR} -F bsd -crv ${.TARGET} s1 a1.o ${TS_LONGNAME} \
		a2.o "s 3" > /dev/null

.include "${TOP}/mk/elftoolchain.tet.mk"
