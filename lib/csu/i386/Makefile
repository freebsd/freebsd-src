#	from: @(#)Makefile	5.6 (Berkeley) 5/22/91
#	$Id: Makefile,v 1.13 1995/02/07 13:27:28 jkh Exp $

CFLAGS+=	-DLIBC_SCCS -DDYNAMIC
OBJS=		crt0.o gcrt0.o c++rt0.o
CLEANFILES+=	a.out
MAN3+=		dlopen.3

all: ${OBJS}

crt0.o: crt0.c
	${CC} ${CFLAGS} -c -DCRT0 ${.ALLSRC} -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

c++rt0.o: c++rt0.c
	${CC} ${CFLAGS} -fpic -c ${.ALLSRC}
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

gcrt0.o: crt0.c
	${CC} ${CFLAGS} -c -DMCRT0 ${.ALLSRC} -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

install:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
	    ${DESTDIR}/usr/lib
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/dlfcn.h \
	    ${DESTDIR}/usr/include

depend lint tags:

.include <bsd.prog.mk>
