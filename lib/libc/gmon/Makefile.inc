#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/gmon/Makefile.inc,v 1.7 1999/08/27 23:59:16 peter Exp $

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
