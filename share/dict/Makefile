#	@(#)Makefile	8.1 (Berkeley) 6/5/93

# XXX MISSING:	connectives words
FILES=	README propernames web2 web2a
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/dict

.include <bsd.prog.mk>
