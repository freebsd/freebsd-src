#	@(#)Makefile	5.10 (Berkeley) 5/11/90

PROG=	restore
SRCS=	main.c interactive.c restore.c dirs.c symtab.c tape.c utilities.c
ROBJS=	main.o interactive.o restore.o dirs.o symtab.o rtape.o utilities.o \
	dumprmt.o
MAN8=	restore.8 rrestore.8
.PATH:	${.CURDIR}/../dump
CLEANFILES+=dumprmt.o rtape.o rrestore

all: rrestore

rrestore: ${ROBJS} ${LIBC}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${ROBJS}

rtape.o: tape.c ${LIBC}
	${CC} ${CFLAGS} -c -DRRESTORE ${.CURDIR}/tape.c -o ${.TARGET}

afterinstall:
	install ${COPY} ${STRIP} -o root -g ${BINGRP} -m 4755 rrestore \
	    ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
