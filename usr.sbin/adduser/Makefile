# TODO:
# rename adduser adduser.sh
# rename adduser.1 adduser.8

MAN1=	adduser.1

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/adduser ${DESTDIR}${BINDIR}/adduser

.include <bsd.prog.mk>
