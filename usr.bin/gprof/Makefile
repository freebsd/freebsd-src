#	@(#)Makefile	5.17 (Berkeley) 5/11/90

PROG=	gprof
SRCS=	gprof.c arcs.c dfn.c lookup.c ${MACHINE}.c hertz.c \
	printgprof.c printlist.c
CFLAGS+=-I${.CURDIR}/../../lib/csu.${MACHINE}

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/gprof.flat ${.CURDIR}/gprof.callg \
	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
