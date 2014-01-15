# $Id: Makefile 1156 2010-09-02 08:55:45Z jkoshy $

TOP=	../../../..

TS_SRCS=		next.m4
TS_DATA=		a1.ar a1.o a2.ar a2.o a3.ar
TS_LONGNAME=		s------------------------2
CLEANFILES+=		a1.c a2.c s1 ${TS_LONGNAME} "s 3"

s1:
	echo 'This is s1.' > ${.TARGET}
${TS_LONGNAME}:
	echo ${TS_LONGNAME} > ${.TARGET}

# a1.ar contains short names only; a2.ar contains long names; a3.ar contains ELF binaries
# and a symbol table.

a1.c:	.SILENT
	echo "int a1;" > ${.TARGET}
a2.c:	.SILENT
	echo "int a2;" > ${.TARGET}

a1.ar:	s1 .SILENT
	rm -f ${.TARGET}
	echo 's-3.' > "s 3"
	${AR} crv ${.TARGET} s1 "s 3" > /dev/null

a2.ar:	s1 ${TS_LONGNAME} .SILENT
	rm -f ${.TARGET}
	${AR} crv ${.TARGET} ${.ALLSRC} > /dev/null

a3.ar:	s1 ${TS_LONGNAME} a1.o a2.o .SILENT
	rm -f ${.TARGET}
	${AR} crv ${.TARGET} ${.ALLSRC} > /dev/null

.include "${TOP}/mk/elftoolchain.tet.mk"
