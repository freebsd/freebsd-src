NOMAN=	noman
CLEANFILES=	catman

beforeinstall: catman
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	     ${.CURDIR}/catman ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>

catman: catman.sh
	sed -e 's,%compress%,${compress},' \
	-e 's,%compext%,${compext},' \
	-e 's,%zcat%,${zcat},' \
	   ${.CURDIR}/catman.sh > catman

