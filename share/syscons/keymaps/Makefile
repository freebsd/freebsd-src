# $Id: Makefile,v 1.18 1997/02/22 13:56:38 peter Exp $

KEYMAPS=  INDEX.keymaps
KEYMAPS+= be.iso.kbd
KEYMAPS+= br275.iso.kbd br275.cp850.kbd
KEYMAPS+= danish.iso.kbd danish.cp865.kbd
KEYMAPS+= fr.iso.kbd
KEYMAPS+= german.iso.kbd german.cp850.kbd
KEYMAPS+= it.iso.kbd
KEYMAPS+= jp.106.kbd jp.106x.kbd
KEYMAPS+= norwegian.iso.kbd
KEYMAPS+= ru.koi8-r.kbd ru.koi8-r.shift.kbd ru.cp866.kbd
KEYMAPS+= spanish.iso.kbd
KEYMAPS+= swedish.iso.kbd swedish.cp850.kbd
KEYMAPS+= swissgerman.iso.kbd
KEYMAPS+= uk.iso.kbd uk.cp850.kbd
KEYMAPS+= us.iso.kbd us.dvorak.kbd us.dvorakx.kbd

MAPSDIR = /usr/share/syscons/keymaps
NOMAN = noman
NOOBJ = noobj

install:
	cd ${.CURDIR} && \
	       ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${KEYMAPS} \
			${DESTDIR}${MAPSDIR}

.include <bsd.prog.mk>
