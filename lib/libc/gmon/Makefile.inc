#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
#	$Id: Makefile.inc,v 1.6 1997/12/19 21:53:35 bde Exp $

# gmon sources
.PATH: ${.CURDIR}/../libc/gmon

SRCS+=	gmon.c mcount.c

.if ${LIB} == "c"
MAN3+=	moncontrol.3

MLINKS+=moncontrol.3 monstartup.3
.endif

# mcount cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po
