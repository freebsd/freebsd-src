MAN8=   adduser.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/adduser.perl ${DESTDIR}${BINDIR}/adduser

.include <bsd.prog.mk>
