#	from: @(#)Makefile	5.6 (Berkeley) 5/22/91
#	$Id: Makefile,v 1.25.2.2 1997/04/30 03:13:33 msmith Exp $

CFLAGS+=        -DLIBC_SCCS -fno-omit-frame-pointer -I${.CURDIR}
OBJS=		crt0.o c++rt0.o gcrt0.o scrt0.o sgcrt0.o
CLEANFILES+=	a.out
MAN3+=		dlopen.3
MLINKS+=	dlopen.3 dlsym.3 \
		dlopen.3 dlerror.3 \
		dlopen.3 dlclose.3

all: ${OBJS}

crt0.o: crt0.c
	${CC} ${CFLAGS} -c -DCRT0 -DDYNAMIC ${.CURDIR}/crt0.c -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv -f a.out ${.TARGET}

c++rt0.o: c++rt0.c
	${CC} ${CFLAGS} -fpic -c ${.CURDIR}/c++rt0.c
	@${LD} -x -r ${.TARGET}
	@mv -f a.out ${.TARGET}

#
# gcrt0.o doesn't really depend on crt0.o, but this is the easiest way
# to get the dependencies mostly correct.
#
gcrt0.o: crt0.o
	${CC} ${CFLAGS} -c -DMCRT0 -DDYNAMIC ${.CURDIR}/crt0.c -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv -f a.out ${.TARGET}

# dependencies fudged as for gcrt0.o
scrt0.o: crt0.o
	${CC} ${CFLAGS} -c -DCRT0 ${.CURDIR}/crt0.c -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv -f a.out ${.TARGET}

# dependencies fudged as for gcrt0.o
sgcrt0.o: scrt0.o
	${CC} ${CFLAGS} -c -DMCRT0 ${.CURDIR}/crt0.c -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv -f a.out ${.TARGET}

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/dlfcn.h \
		${DESTDIR}/usr/include

realinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
		${DESTDIR}/usr/lib

depend:	.depend

.depend:	crt0.c c++rt0.c
	rm -f .depend
	mkdep ${CFLAGS} -DCRT0 -DDYNAMIC ${.CURDIR}/crt0.c
	mkdep -a ${CFLAGS} ${.CURDIR}/c++rt0.c

lint tags:

.include <bsd.prog.mk>
