#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	ar
CFLAGS+=-I${.CURDIR}
SRCS=	append.c ar.c archive.c contents.c delete.c extract.c misc.c \
	move.c print.c replace.c 
MAN1=	ar.0
CLEANFILES=ar.5.0

ar.0: ar.5.0

afterinstall:
	install -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} ar.5.0 \
	    ${DESTDIR}${MANDIR}5/ar.0

.include <bsd.prog.mk>
