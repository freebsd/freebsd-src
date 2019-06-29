# $Id$

TOP=	../../../..

TS_SRCS=	rand.m4
TS_DATA=	a.ar s1 s2
TS_FILES=	empty-file.ar missing-file.ar

s1:	.SILENT
	echo 'This is s1.' > ${.TARGET}
s2:	.SILENT
	echo 's2.' > ${.TARGET}

a.ar:	s1 s2 .SILENT
	rm -f ${.TARGET}
	${AR} crv ${.TARGET} s1 s2 > /dev/null

.include "${TOP}/mk/elftoolchain.tet.mk"
