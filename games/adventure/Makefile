#	@(#)Makefile	8.1 (Berkeley) 6/12/93
# $FreeBSD: src/games/adventure/Makefile,v 1.7.6.1 2001/04/25 09:28:42 ru Exp $

PROG=	adventure
SRCS=	main.c init.c done.c save.c subr.c vocab.c wizard.c io.c data.c crc.c
MAN=	adventure.6
CFLAGS+=-traditional-cpp
HIDEGAME=hidegame
CLEANFILES=data.c setup setup.o

build-tools: setup

data.c: glorkz setup
	./setup ${.CURDIR}/glorkz > data.c

setup: setup.o
	${CC} -static ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

.include <bsd.prog.mk>
