# $FreeBSD$

MAN1=		afmtodit.1

CLEANFILES+=	${MAN1}

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${DIST_DIR}/afmtodit.pl ${DESTDIR}${BINDIR}/afmtodit

.include "../Makefile.cfg"
.include <bsd.prog.mk>
