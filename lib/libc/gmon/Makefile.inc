#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/gmon/Makefile.inc,v 1.9 2002/11/18 09:50:55 ru Exp $

# gmon sources
.PATH: ${.CURDIR}/gmon

SRCS+=	gmon.c mcount.c

MAN+=	moncontrol.3

MLINKS+=moncontrol.3 monstartup.3

# mcount cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po
