#	from: @(#)Makefile	5.6 (Berkeley) 5/22/91
# $FreeBSD$

CFLAGS+=        -DLIBC_SCCS -fno-omit-frame-pointer \
		-I${.CURDIR}/../../libc/include
OBJS=		crt0.o c++rt0.o gcrt0.o scrt0.o sgcrt0.o
CLEANFILES=	a.out crt0.o.tmp c++rt0.o.tmp gcrt0.o.tmp scrt0.o.tmp \
		sgcrt0.o.tmp

all: ${OBJS}

crt0.o: crt0.c
	${CC} ${CFLAGS} -c -DCRT0 -DDYNAMIC ${.CURDIR}/crt0.c -o ${.TARGET}
	${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

c++rt0.o: c++rt0.c
	${CC} ${CFLAGS} -fpic -c ${.CURDIR}/c++rt0.c
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

#
# gcrt0.o doesn't really depend on crt0.o, but this is the easiest way
# to get the dependencies mostly correct.
#
gcrt0.o: crt0.o
	${CC} ${CFLAGS} -c -DMCRT0 -DDYNAMIC ${.CURDIR}/crt0.c -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

# dependencies fudged as for gcrt0.o
scrt0.o: crt0.o
	${CC} ${CFLAGS} -c -DCRT0 ${.CURDIR}/crt0.c -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

# dependencies fudged as for gcrt0.o
sgcrt0.o: scrt0.o
	${CC} ${CFLAGS} -c -DMCRT0 ${.CURDIR}/crt0.c -o ${.TARGET}
	@${LD} -o ${.TARGET}.tmp -x -r ${.TARGET}
	@mv ${.TARGET}.tmp ${.TARGET}

realinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
		${DESTDIR}${LIBDIR}

depend:	.depend

.depend:	crt0.c c++rt0.c
	rm -f .depend
	mkdep ${CFLAGS} -DCRT0 -DDYNAMIC ${.CURDIR}/crt0.c
	mkdep -a ${CFLAGS} ${.CURDIR}/c++rt0.c

cleandepend:
	rm -f .depend

lint tags:

.include <bsd.prog.mk>
