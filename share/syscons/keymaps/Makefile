# $Id: Makefile,v 1.33.2.2 1999/06/23 08:21:40 yokota Exp $

KEYMAPS=  INDEX.keymaps
KEYMAPS+= be.iso.kbd be.iso.acc.kbd
KEYMAPS+= br275.iso.kbd br275.iso.acc.kbd br275.cp850.kbd
KEYMAPS+= ce.iso2.kbd
KEYMAPS+= cz.iso2.kbd
KEYMAPS+= danish.iso.kbd danish.cp865.kbd
KEYMAPS+= finnish.iso.kbd finnish.cp850.kbd
KEYMAPS+= fr.iso.kbd fr.iso.acc.kbd
KEYMAPS+= german.iso.kbd german.cp850.kbd
KEYMAPS+= hr.iso.kbd
KEYMAPS+= hu.iso2.101keys.kbd hu.iso2.102keys.kbd
KEYMAPS+= icelandic.iso.kbd icelandic.iso.acc.kbd
KEYMAPS+= it.iso.kbd
KEYMAPS+= jp.106.kbd jp.106x.kbd
KEYMAPS+= lat-amer.kbd
KEYMAPS+= norwegian.iso.kbd
KEYMAPS+= pl_PL.ISO_8859-2.kbd 
KEYMAPS+= pt.iso.kbd pt.iso.acc.kbd
KEYMAPS+= ru.koi8-r.kbd ru.koi8-r.shift.kbd ru.cp866.kbd
KEYMAPS+= si.iso.kbd
KEYMAPS+= sk.iso2.kbd
KEYMAPS+= spanish.iso.kbd spanish.iso.acc.kbd
KEYMAPS+= swedish.iso.kbd swedish.cp850.kbd
KEYMAPS+= swissfrench.iso.kbd
KEYMAPS+= swissgerman.iso.kbd
KEYMAPS+= uk.iso.kbd uk.cp850.kbd
KEYMAPS+= us.iso.kbd us.dvorak.kbd us.dvorakx.kbd us.emacs.kbd us.unix.kbd

MAPSDIR = ${SHAREDIR}/syscons/keymaps
NOMAN = noman
NOOBJ = noobj

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${KEYMAPS} \
	    ${DESTDIR}${MAPSDIR}

.include <bsd.prog.mk>
