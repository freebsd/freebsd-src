SCRMAPS = koi82alt.scr

SCRDIR = /usr/share/syscons/scrnmaps

install:
	cd ${.CURDIR}; \
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${SCRMAPS} \
		${DESTDIR}${SCRDIR}

.include <bsd.prog.mk>
