MAN1=	htags.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/htags.pl ${DESTDIR}${BINDIR}/htags

.include <bsd.prog.mk>
