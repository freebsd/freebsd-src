#	@(#)Makefile	5.5 (Berkeley) 1/18/91

PROG=	fish
MAN6=	fish.6
HIDEGAME=hidegame

beforeinstall: 
	install -c -o bin -g bin -m 444 ${.CURDIR}/fish.instr ${DESTDIR}/usr/share/games

.include <bsd.prog.mk>
