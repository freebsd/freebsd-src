#	$Id: Makefile,v 1.2 1995/08/06 12:22:38 bde Exp $
#
all depend lint tags:

FILES= values.h
NOOBJ=	noobj

beforeinstall:
.for i in ${FILES}
	cd ${.CURDIR} ; \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 $i \
		${DESTDIR}/usr/include/$i
.endfor

.include <bsd.prog.mk>
