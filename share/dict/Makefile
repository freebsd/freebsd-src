#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$Id$

# XXX MISSING:	connectives words
FILES=	README propernames web2 web2a
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${FILES} \
	    ${DESTDIR}${BINDIR}/dict
	(cd ${DESTDIR}${BINDIR}/dict; rm -f words; ln -s web2 words)

.include <bsd.prog.mk>
