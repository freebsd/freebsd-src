#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD$

# gmon sources
.PATH: ${.CURDIR}/gmon

SRCS+=	gmon.c mcount.c

MAN+=	moncontrol.3

MLINKS+=moncontrol.3 monstartup.3

.if ${MACHINE_ARCH} == amd64
# mcount needs to be compiled with frame pointers and without profiling
mcount.po: mcount.c
	${CC} ${CFLAGS} -fno-omit-frame-pointer -c ${.IMPSRC} -o ${.TARGET}
.else
# mcount cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po
.endif
