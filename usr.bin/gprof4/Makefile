# This was cloned from the Makefile for gprof by changing PROG from gprof
# to gprof4, adding NOMAN and PATH, adding -DGPROF4 to CFLAGS and deleting
# beforeinstall.

#	@(#)Makefile	5.17 (Berkeley) 5/11/90
#	$FreeBSD$

PROG=	gprof4
NOMAN=	noman
SRCS=	gprof.c aout.c arcs.c dfn.c elf.c lookup.c ${MACHINE_ARCH}.c hertz.c \
	printgprof.c printlist.c kernel.c
CFLAGS+=-DGPROF4
.PATH:	${.CURDIR}/../../usr.bin/gprof

.include <bsd.prog.mk>
