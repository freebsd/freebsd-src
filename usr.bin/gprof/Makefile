#	@(#)Makefile	8.1 (Berkeley) 6/29/93

PROG=	gprof
SRCS=	gprof.c aout.c arcs.c dfn.c elf.c lookup.c ${MACHINE_ARCH}.c hertz.c \
	printgprof.c printlist.c

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/gprof.flat ${.CURDIR}/gprof.callg \
	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
