#	@(#)Makefile	5.3 (Berkeley) 6/8/90

FILES=	dot.cshrc dot.login dot.mailrc dot.profile dot.rhosts
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/skel

.include <bsd.prog.mk>
