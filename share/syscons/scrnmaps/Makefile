OLDSCRMAPS = koi8-r2alt.scr

SCRMAPS = koi8-r2alt.scm

SCRMAPS_MK = ${OLDSCRMAPS:R:S/$/.mk/g}
SCRDIR = /usr/share/syscons/scrnmaps
CLEANFILES+= ${OLDSCRMAPS} ${SCRMAPS_MK}

NOMAN = noman

all: ${OLDSCRMAPS}

install:
	cd ${.CURDIR}; \
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${SCRMAPS} \
		${DESTDIR}${SCRDIR}
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${OLDSCRMAPS} \
		${DESTDIR}${SCRDIR}

${OLDSCRMAPS}: ${.TARGET:R}.mk
	./${.TARGET:R}.mk ${.TARGET}

${SCRMAPS_MK}: ${.TARGET:R} mkscrfil.c
	${CC} ${CFLAGS} -I${.CURDIR} -DFIL=\"${.TARGET:R}\" \
			-o ${.TARGET} ${.CURDIR}/mkscrfil.c

.include <bsd.prog.mk>
