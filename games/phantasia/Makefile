#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	phantasia
SRCS=	main.c fight.c io.c interplayer.c gamesupport.c misc.c phantglobs.c
DPADD=	${LIBM} ${LIBCURSES} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-lm -lcurses -ltermcap -lcompat
HIDEGAME=hidegame
MAN6=	phantasia.6
CLEANFILES+=map setup setup.o
CLEANFILES+= characs gold lastdead mess monsters motd scoreboard void

all: setup phantasia ${MAN6}

setup: phantglobs.o setup.o monsters.asc ${LIBM}
	${CC} phantglobs.o setup.o -o ${.TARGET} -lm

beforeinstall:
	./setup -m ${.CURDIR}/monsters.asc
	${INSTALL} ${COPY} -m 660 -o games -g bin gold ${DESTDIR}/var/games/phantasia
	${INSTALL} ${COPY} -m 660 -o games -g bin lastdead ${DESTDIR}/var/games/phantasia
	${INSTALL} ${COPY} -m 660 -o games -g bin mess ${DESTDIR}/var/games/phantasia
	${INSTALL} ${COPY} -m 660 -o games -g bin monsters ${DESTDIR}/var/games/phantasia
	${INSTALL} ${COPY} -m 660 -o games -g bin motd ${DESTDIR}/var/games/phantasia
	${INSTALL} ${COPY} -m 660 -o games -g bin characs ${DESTDIR}/var/games/phantasia
	${INSTALL} ${COPY} -m 660 -o games -g bin scoreboard ${DESTDIR}/var/games/phantasia
	${INSTALL} ${COPY} -m 660 -o games -g bin void ${DESTDIR}/var/games/phantasia

# Make Phantasia map.  Change the map commands reflect your ${INSTALL}ation.
# PLOTDEVICE is used for plotting the map.  Change as appropriate.

map: map.c
	${CC} -O ${.CURDIR}/map.c -lplot -o ${.TARGET}
	./map | plot > /dev/tty

phantasia.0: phantasia.6
	tbl ${.CURDIR}/phantasia.6 | nroff -man > ${.TARGET}

.include <bsd.prog.mk>
