#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	phantasia
SRCS=	main.c fight.c io.c interplayer.c gamesupport.c misc.c phantglobs.c
DPADD=	${LIBM} ${LIBCURSES} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-lm -lcurses -ltermcap -lcompat
DATAFILES=characs gold lastdead mess monsters motd scoreboard void
HIDEGAME=hidegame
MAN6=	phantasia.6
CLEANFILES=${DATAFILES} map setup setup.o stamp.setuprun

all: stamp.setuprun

stamp.setuprun: monsters.asc setup
	./setup -m ${.CURDIR}/monsters.asc
	touch ${.TARGET}

setup: phantglobs.o setup.o ${LIBM}
	${CC} ${CFLAGS} phantglobs.o setup.o -o ${.TARGET} -lm

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 660 \
	    ${DATAFILES} ${DESTDIR}/var/games/phantasia

# Make Phantasia map.  Change the map commands reflect your installation.
# PLOTDEVICE is used for plotting the map.  Change as appropriate.
map: map.c
	${CC} -O ${.CURDIR}/map.c -lplot -o ${.TARGET}
	./map | plot > /dev/tty

.include <bsd.prog.mk>
