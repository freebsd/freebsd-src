# This was cloned from the Makefile for gprof by changing PROG from gprof
# to gprof4, adding NOMAN and PATH, adding -DGPROF4 to CFLAGS and deleting
# beforeinstall.

#	@(#)Makefile	5.17 (Berkeley) 5/11/90

PROG=	gprof4
NOMAN=	noman
SRCS=	gprof.c arcs.c dfn.c lookup.c ${MACHINE}.c hertz.c \
	printgprof.c printlist.c
CFLAGS+=-DGPROF4
.PATH:	${.CURDIR}/../../usr.bin/gprof

.include <bsd.prog.mk>
