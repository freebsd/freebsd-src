#	@(#)Makefile	8.1 (Berkeley) 5/31/93

SUBDIR=	cno phx
MAN6=	ching.6

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/ching.sh ${DESTDIR}/usr/games/ching
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/hexagrams \
	    ${.CURDIR}/macros ${DESTDIR}/usr/share/games/ching

.include <bsd.prog.mk>
