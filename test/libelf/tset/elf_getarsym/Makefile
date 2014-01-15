# $Id: Makefile 1323 2010-12-26 05:50:57Z jkoshy $

TOP=	../../../..

TS_SRCS=		getarsym.m4

# These names must match those in the test case code.
TS_DATA=		a.ar a-bsd.ar a2.ar a2-bsd.ar a1.o a2.o
TS_LONGNAME=		"s------------------------2"
CLEANFILES+=		a1.c a2.c s1 ${TS_LONGNAME} "s 3"

a1.c:	.SILENT
	echo "int a1;" > ${.TARGET}
a2.c:	.SILENT
	echo "int a2;" > ${.TARGET}

s1:	.SILENT
	echo 'This is s1.' > ${.TARGET}
${TS_LONGNAME}:	.SILENT
	echo 's2.' > ${.TARGET}

a.ar:	a1.o a2.o ${TS_LONGNAME} s1 .SILENT
	rm -f ${.TARGET}
	echo 's-3.' > "s 3"
	${AR} crv ${.TARGET} s1 a1.o ${TS_LONGNAME} a2.o "s 3" > /dev/null

a2.ar:	s1 ${TS_LONGNAME} .SILENT
	rm -f ${.TARGET}
	${AR} crv ${.TARGET} ${.ALLSRC} > /dev/null

a-bsd.ar:	a.ar .SILENT
	rm -f ${.TARGET}
	${ELFTOOLCHAIN_AR} -F bsd -crv ${.TARGET} s1 a1.o ${TS_LONGNAME} \
		a2.o "s 3" > /dev/null

a2-bsd.ar:	a2.ar .SILENT
	rm -f ${.TARGET}
	${ELFTOOLCHAIN_AR} -F bsd -crv ${.TARGET} ${.ALLSRC} > /dev/null

.include "${TOP}/mk/elftoolchain.tet.mk"
