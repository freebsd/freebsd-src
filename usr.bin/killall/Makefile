MAN1=	killall.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/killall.pl ${DESTDIR}${BINDIR}/killall

.include <bsd.prog.mk>
