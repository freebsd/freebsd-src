#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD$

# gmon sources
.PATH: ${.CURDIR}/../libc/gmon

SRCS+=	gmon.c mcount.c

.if ${LIB} == "c"
MAN+=	moncontrol.3

MLINKS+=moncontrol.3 monstartup.3
.endif

# mcount cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po
