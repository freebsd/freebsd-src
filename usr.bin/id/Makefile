#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	id
MAN1=   id.0 groups.0 whoami.0

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/groups.sh ${DESTDIR}/usr/bin/groups
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/whoami.sh ${DESTDIR}/usr/bin/whoami
	${MINSTALL} groups.0 ${DESTDIR}${MANDIR}1/groups.0
	${MINSTALL} whoami.0 ${DESTDIR}${MANDIR}1/whoami.0

.include <bsd.prog.mk>
