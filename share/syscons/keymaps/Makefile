KEYMAPS=danish.cp865.map danish.iso.map german.cp850.map german.iso.map \
	ru.koi8-r.map swedish.cp850.map swedish.iso.map uk.cp850.map uk.iso.map \
	us.iso.map
KEYMAPS_MK = ${KEYMAPS:R:S/$/.mk/g}
MAPSDIR = /usr/share/syscons/keymaps
NOMAN = noman
CLEANFILES += ${KEYMAPS} ${KEYMAPS_MK}


all: ${KEYMAPS}

install:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${KEYMAPS} \
			${DESTDIR}${MAPSDIR}

clean:
	-rm -f ${KEYMAPS} ${KEYMAPS_MK} core *.o 

${KEYMAPS}: ${.TARGET:R}.mk
	./${.TARGET:R}.mk ${.TARGET}

${KEYMAPS_MK}: ${.TARGET:R} mkkbdfil.c
	${CC} ${CFLAGS} -I${.CURDIR} -DFIL=\"${.TARGET:R}\" \
			-o ${.TARGET} ${.CURDIR}/mkkbdfil.c

.include <bsd.prog.mk>
