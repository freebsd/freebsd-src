MAN1=	makewhatis.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/makewhatis.perl ${DESTDIR}${BINDIR}/makewhatis

.include <bsd.prog.mk>
