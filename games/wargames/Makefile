#	@(#)Makefile	8.1 (Berkeley) 5/31/93

NOOBJ=	noobj
all wargames clean cleandir depend lint tags:

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} wargames.sh \
	    ${DESTDIR}/usr/games/wargames

.include <bsd.prog.mk>
