...using script...
CVS: up -A -p Makefile
MAN1 = c89.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	     ${.CURDIR}/c89.sh ${DESTDIR}${BINDIR}/c89

.include <bsd.prog.mk>
