#	from: @(#)Makefile	5.6 (Berkeley) 5/22/91
#	$Id: Makefile,v 1.36 1998/02/11 04:57:10 jdp Exp $

CFLAGS+=        -DLIBC_SCCS -fno-omit-frame-pointer
OBJS=		crt0.o c++rt0.o gcrt0.o scrt0.o sgcrt0.o
CLEANFILES+=	a.out

all: ${OBJS}

crt0.o: crt0.c
	${CC} ${CFLAGS} -c -DCRT0 -DDYNAMIC ${.CURDIR}/crt0.c -o ${.TARGET}
	${LD} -O ${.TARGET} -x -r ${.TARGET}

c++rt0.o: c++rt0.c
	${CC} ${CFLAGS} -fpic -c ${.CURDIR}/c++rt0.c
	@${LD} -O ${.TARGET} -x -r ${.TARGET}

#
# gcrt0.o doesn't really depend on crt0.o, but this is the easiest way
# to get the dependencies mostly correct.
#
gcrt0.o: crt0.o
	${CC} ${CFLAGS} -c -DMCRT0 -DDYNAMIC ${.CURDIR}/crt0.c -o ${.TARGET}
	${LD} -O ${.TARGET} -x -r ${.TARGET}

# dependencies fudged as for gcrt0.o
scrt0.o: crt0.o
	${CC} ${CFLAGS} -c -DCRT0 ${.CURDIR}/crt0.c -o ${.TARGET}
	${LD} -O ${.TARGET} -x -r ${.TARGET}

# dependencies fudged as for gcrt0.o
sgcrt0.o: scrt0.o
	${CC} ${CFLAGS} -c -DMCRT0 ${.CURDIR}/crt0.c -o ${.TARGET}
	${LD} -O ${.TARGET} -x -r ${.TARGET}

realinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
		${DESTDIR}/${LIBDIR}

depend:	.depend

.depend:	crt0.c c++rt0.c
	rm -f .depend
	mkdep ${CFLAGS} -DCRT0 -DDYNAMIC ${.CURDIR}/crt0.c
	mkdep -a ${CFLAGS} ${.CURDIR}/c++rt0.c

lint tags:

.include <bsd.prog.mk>
