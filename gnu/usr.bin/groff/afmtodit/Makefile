# $FreeBSD: src/gnu/usr.bin/groff/afmtodit/Makefile,v 1.1.2.1 2000/12/07 09:56:59 ru Exp $

MAN1=		afmtodit.1

CLEANFILES+=	${MAN1}

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${DIST_DIR}/afmtodit.pl ${DESTDIR}${BINDIR}/afmtodit

.include "../Makefile.cfg"
.include <bsd.prog.mk>
