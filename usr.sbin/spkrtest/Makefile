NOMAN= noman

clean depend lint tags obj:

install:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/spkrtest.sh ${DESTDIR}/${BINDIR}/spkrtest

.include <bsd.prog.mk>
