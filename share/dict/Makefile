#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$Id: Makefile,v 1.4.2.1 1998/03/06 13:22:46 jkh Exp $

# XXX MISSING:	connectives words
FILES=	README propernames web2 web2a
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${FILES} \
	    ${DESTDIR}${BINDIR}/dict
	(cd ${DESTDIR}${BINDIR}/dict; rm -f words; ln -s web2 words)

.include <bsd.prog.mk>
