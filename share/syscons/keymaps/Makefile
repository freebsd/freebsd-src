KEYMAPS=danish.cp865.kbd danish.iso.kbd german.cp850.kbd german.iso.kbd \
	 swedish.cp850.kbd swedish.iso.kbd uk.cp850.kbd uk.iso.kbd \
	 us.iso.kbd fr.iso.kbd ru.koi8-r.kbd ru.koi8-r.shift.kbd

MAPSDIR = /usr/share/syscons/keymaps
NOMAN = noman

all:;

clean:;

install:
	cd ${.CURDIR}; \
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${KEYMAPS} \
			${DESTDIR}${MAPSDIR}

.include <bsd.prog.mk>
