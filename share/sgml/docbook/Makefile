#	$Id: Makefile,v 1.3.2.1 1997/02/07 02:29:32 jfieber Exp $

FILES=	cals-tbl.dtd dbgenent.mod dbhier.mod dbpool.mod
FILES+=	docbook.dcl docbook.dtd
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DESTDIR}${BINDIR}/${.CURDIR:T}

.include <bsd.prog.mk>


