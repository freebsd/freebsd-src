#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	id
MAN1=   id.1 groups.1 whoami.1

# XXX BROKEN:	afterinstall:
afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/groups.sh ${DESTDIR}${BINDIR}/groups
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/whoami.sh ${DESTDIR}${BINDIR}/whoami

.include <bsd.prog.mk>
