# $Id: Makefile,v 1.23 1997/11/07 15:55:06 ache Exp $

KEYMAPS=  INDEX.keymaps
KEYMAPS+= be.iso.kbd
KEYMAPS+= br275.iso.kbd br275.cp850.kbd
KEYMAPS+= danish.iso.kbd danish.cp865.kbd
KEYMAPS+= fr.iso.kbd
KEYMAPS+= german.iso.kbd german.cp850.kbd
KEYMAPS+= it.iso.kbd
KEYMAPS+= jp.106.kbd jp.106x.kbd
KEYMAPS+= lat-amer.kbd
KEYMAPS+= norwegian.iso.kbd
KEYMAPS+= pl_PL.ISO_8859-2.kbd 
KEYMAPS+= ru.koi8-r.kbd ru.koi8-r.shift.kbd ru.cp866.kbd
KEYMAPS+= si.iso.kbd
KEYMAPS+= spanish.iso.kbd
KEYMAPS+= swedish.iso.kbd swedish.cp850.kbd
KEYMAPS+= swissgerman.iso.kbd
KEYMAPS+= uk.iso.kbd uk.cp850.kbd
KEYMAPS+= us.iso.kbd us.dvorak.kbd us.dvorakx.kbd

MAPSDIR = ${SHAREDIR}/syscons/keymaps
NOMAN = noman
NOOBJ = noobj

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${KEYMAPS} \
	    ${DESTDIR}${MAPSDIR}

.include <bsd.prog.mk>
