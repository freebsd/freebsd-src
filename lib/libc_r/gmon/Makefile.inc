#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93

# gmon sources
.PATH: ${.CURDIR}/../libc/gmon

SRCS+=	gmon.c mcount.c

# mcount cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po
