# $FreeBSD$

# gmon sources
.PATH: ${.CURDIR}/gmon

SRCS += \
	gmon.c \
	mcount.c

SYM_MAPS += ${.CURDIR}/gmon/Symbol.map

# mcount cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po
