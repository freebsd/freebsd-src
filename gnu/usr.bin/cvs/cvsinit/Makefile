#	$Id$

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/cvsinit.sh ${DESTDIR}${BINDIR}/cvsinit

.include "../../Makefile.inc"
.include <bsd.prog.mk>
