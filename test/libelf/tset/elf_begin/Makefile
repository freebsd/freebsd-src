# $Id: Makefile 3704 2019-03-02 20:41:12Z jkoshy $

TOP=	../../../..

TS_SRCS=	begin.m4
TS_FILES=	entry-too-large.ar
TS_DATA=	check_elf.msb32 check_elf.lsb32 check_elf.msb64 \
		check_elf.lsb64 a.ar a-bsd.ar a.o zero
CLEANFILES+=	a.c

a.c:	.SILENT
	echo "int dummy;" > a.c

a.ar:	a.o .SILENT
	${AR} crv ${.TARGET} ${.ALLSRC}	> /dev/null

a-bsd.ar: a.o .SILENT
	rm -f ${.TARGET}
	${ELFTOOLCHAIN_AR} -F bsd -crv ${.TARGET} ${.ALLSRC} > /dev/null

zero: .SILENT
	rm -f ${.TARGET}; touch ${.TARGET}

.include "${TOP}/mk/elftoolchain.tet.mk"
