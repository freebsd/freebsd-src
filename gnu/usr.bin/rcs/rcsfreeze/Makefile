MAN1=	rcsfreeze.0

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/rcsfreeze.sh ${DESTDIR}${BINDIR}/rcsfreeze

.include "../../Makefile.inc"
.include <bsd.prog.mk>
