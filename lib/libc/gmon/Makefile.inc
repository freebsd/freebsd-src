#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
#	$Id: Makefile.inc,v 1.4 1997/05/03 03:49:55 jb Exp $

# gmon sources
.PATH: ${.CURDIR}/../libc/gmon

SRCS+=	gmon.c mcount.c

# Only build man pages with libc.
.if ${LIB} == "c"
MAN3+=	gmon/moncontrol.3
MLINKS+= moncontrol.3 monstartup.3
.endif

# mcount cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po
