#	@(#)Makefile	8.1 (Berkeley) 6/12/93

PROG=	adventure
SRCS=	main.c init.c done.c save.c subr.c vocab.c wizard.c io.c data.c crc.c
MAN6=	adventure.6
CFLAGS+=-traditional-cpp
HIDEGAME=hidegame
CLEANFILES+=setup data.c

build-tools: setup

data.c: glorkz setup
	./setup ${.CURDIR}/glorkz > data.c

setup: setup.o
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

.include <bsd.prog.mk>
