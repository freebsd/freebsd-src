#	$Id: Makefile,v 1.1 1997/01/04 22:38:34 jfieber Exp $

FILES=	cals-tbl.dtd dbgenent.mod dbhier.mod dbpool.mod
FILES+=	docbook.dcl docbook.dtd
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DESTDIR}${BINDIR}/${.CURDIR:T}

.include <bsd.prog.mk>


