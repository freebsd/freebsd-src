#	from: @(#)Makefile	5.6 (Berkeley) 5/22/91
#	$Id: Makefile,v 1.15.4.1 1995/08/30 07:31:12 davidg Exp $

CFLAGS+=	-DLIBC_SCCS -DDYNAMIC
OBJS=		crt0.o gcrt0.o c++rt0.o
CLEANFILES+=	a.out
MAN3+=		dlopen.3
MLINKS+=	dlopen.3 dlsym.3 \
		dlopen.3 dlerror.3 \
		dlopen.3 dlclose.3

all: ${OBJS}

crt0.o: crt0.c
	${CC} ${CFLAGS} -c -DCRT0 ${.CURDIR}/crt0.c -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

c++rt0.o: c++rt0.c
	${CC} ${CFLAGS} -fpic -c ${.CURDIR}/c++rt0.c
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

#
# gcrt0.o doesn't really depend on crt0.o, but this is the easiest way
# to get the dependencies mostly correct.
#
gcrt0.o: crt0.o
	${CC} ${CFLAGS} -c -DMCRT0 ${.CURDIR}/crt0.c -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

beforeinstall:
	cmp -s ${.CURDIR}/dlfcn.h ${DESTDIR}/usr/include/dlfcn.h || \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/dlfcn.h ${DESTDIR}/usr/include

realinstall:
.for i in ${OBJS}
	cmp -s $i ${DESTDIR}/usr/lib/$i || \
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		$i ${DESTDIR}/usr/lib
.endfor

depend:	.depend

.depend:	crt0.c c++rt0.c
	rm -f .depend
	mkdep ${CFLAGS} -DCRT0 ${.CURDIR}/crt0.c
	mkdep -a ${CFLAGS} ${.CURDIR}/c++rt0.c

lint tags:

.include <bsd.prog.mk>
