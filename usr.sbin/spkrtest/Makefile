MAN8=	spkrtest.8
NOOBJ=  yes

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/spkrtest.pl ${DESTDIR}${BINDIR}/spkrtest

.include <bsd.prog.mk>
