#	from: @(#)Makefile	5.6 (Berkeley) 5/22/91
#	$Id: Makefile,v 1.8 1993/12/24 02:11:37 jkh Exp $

CFLAGS+=	-DLIBC_SCCS -DDYNAMIC
OBJS=		crt0.o gcrt0.o c++rt0.o
CLEANFILES+=	gmon.o moncrt0.o core a.out

all: ${OBJS}

crt0.o: crt0.c
	${CC} ${CFLAGS} -c -DCRT0 ${.ALLSRC} -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

c++rt0.o: c++rt0.c
	${CC} ${CFLAGS} -fpic -c ${.ALLSRC}
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

moncrt0.o: crt0.c
	${CC} ${CFLAGS} -c -DMCRT0 ${.ALLSRC} -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

gcrt0.o: moncrt0.o gmon.o
	${LD} -x -r -o ${.TARGET} moncrt0.o gmon.o

gmon.o: gmon.c gmon.h
	${CC} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${LD} -x -r ${.TARGET}
	mv a.out ${.TARGET}

install:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
	    ${DESTDIR}/usr/lib

depend lint tags:

.include <bsd.prog.mk>
