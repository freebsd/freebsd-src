#	@(#)Makefile	8.1 (Berkeley) 6/12/93
# $FreeBSD$

PROG=	adventure
SRCS=	main.c init.c done.c save.c subr.c vocab.c wizard.c io.c data.c crc.c
MAN=	adventure.6
HIDEGAME=hidegame
CLEANFILES=data.c setup setup.o

WARNS?=	2

build-tools: setup

data.c: glorkz setup
	./setup ${.CURDIR}/glorkz > data.c

setup: setup.o
	${CC} -static ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

.include <bsd.prog.mk>
