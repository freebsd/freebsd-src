MAN8=   adduser.8

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/adduser.sh ${DESTDIR}${BINDIR}/adduser

.include <bsd.prog.mk>
