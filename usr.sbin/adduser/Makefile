MAN8=   adduser.8

clean obj:

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/adduser.perl ${DESTDIR}${BINDIR}/adduser

.include <bsd.prog.mk>
