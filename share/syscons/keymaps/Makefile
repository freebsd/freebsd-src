OLDKEYMAPS=danish.cp865.map danish.iso.map german.cp850.map german.iso.map \
	ru.koi8-r.map swedish.cp850.map swedish.iso.map uk.cp850.map \
	uk.iso.map us.iso.map

KEYMAPS=danish.cp865.kbd danish.iso.kbd german.cp850.kbd german.iso.kbd \
	 swedish.cp850.kbd swedish.iso.kbd uk.cp850.kbd uk.iso.kbd \
	 us.iso.kbd ru.koi8-r.kbd

KEYMAPS_MK = ${OLDKEYMAPS:R:S/$/.mk/g}
MAPSDIR = /usr/share/syscons/keymaps
NOMAN = noman
CLEANFILES += ${OLDKEYMAPS} ${KEYMAPS_MK}


all: ${OLDKEYMAPS}

install:
	cd ${.CURDIR}; \
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${KEYMAPS} \
			${DESTDIR}${MAPSDIR}
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${OLDKEYMAPS} \
			${DESTDIR}${MAPSDIR}

clean:
	-rm -f ${OLDKEYMAPS} ${KEYMAPS_MK} core *.o 

${OLDKEYMAPS}: ${.TARGET:R}.mk
	./${.TARGET:R}.mk ${.TARGET}

${KEYMAPS_MK}: ${.TARGET:R} mkkbdfil.c
	${CC} ${CFLAGS} -I${.CURDIR} -DFIL=\"${.TARGET:R}\" \
			-o ${.TARGET} ${.CURDIR}/mkkbdfil.c

.include <bsd.prog.mk>
