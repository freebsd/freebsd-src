MAN1=	makewhatis.1
# MAN8=	makewhatis.local.8

libexecdir=/usr/libexec
LINKS=${libexecdir}/makewhatis.local ${libexecdir}/catman.local 

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/makewhatis.perl ${DESTDIR}${BINDIR}/makewhatis
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/makewhatis.local.sh \
	    ${DESTDIR}${libexecdir}/makewhatis.local


.include <bsd.prog.mk>
