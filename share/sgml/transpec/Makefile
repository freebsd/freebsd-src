#	$Id: Makefile,v 1.5 1997/05/10 03:26:27 jfieber Exp $

FILES=	roff.cmap roff.sdata linuxdoc-roff.ts
FILES+= html.cmap html.sdata linuxdoc-html.ts
FILES+=	docbook-html.ts
FILES+= transpec.dtd
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DESTDIR}${BINDIR}/${.CURDIR:T}

.include <bsd.prog.mk>
