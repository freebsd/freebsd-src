#       @(#)Makefile    8.1 (Berkeley) 6/5/93
#	$Id$

NOOBJ=  noobj

all clean cleandir depend lint tags:

beforeinstall:
	${INSTALL} -c -o ${INFOOWN} -g ${INFOGRP} -m ${INFOMODE} dir-tmpl \
		${DESTDIR}${INFODIR}/dir-tmpl
.if !exists(${DESTDIR}${INFODIR}/dir)
	${INSTALL} -c -o ${INFOOWN} -g ${INFOGRP} -m ${INFOMODE} \
		 dir-tmpl ${DESTDIR}${INFODIR}/dir
.endif

.include <bsd.prog.mk>
