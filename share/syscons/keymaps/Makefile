# $Id: Makefile,v 1.28 1998/03/28 06:42:42 imp Exp $

KEYMAPS=  INDEX.keymaps
KEYMAPS+= be.iso.kbd
KEYMAPS+= br275.iso.kbd br275.iso.acc.kbd br275.cp850.kbd
KEYMAPS+= danish.iso.kbd danish.cp865.kbd
KEYMAPS+= fr.iso.kbd fr.iso.acc.kbd
KEYMAPS+= german.iso.kbd german.cp850.kbd
KEYMAPS+= icelandic.iso.kbd icelandic.iso.acc.kbd
KEYMAPS+= it.iso.kbd
KEYMAPS+= jp.106.kbd jp.106x.kbd
KEYMAPS+= lat-amer.kbd
KEYMAPS+= norwegian.iso.kbd
KEYMAPS+= pl_PL.ISO_8859-2.kbd 
KEYMAPS+= ru.koi8-r.kbd ru.koi8-r.shift.kbd ru.cp866.kbd
KEYMAPS+= si.iso.kbd
KEYMAPS+= spanish.iso.kbd spanish.iso.acc.kbd
KEYMAPS+= swedish.iso.kbd swedish.cp850.kbd
KEYMAPS+= swissfrench.iso.kbd
KEYMAPS+= swissgerman.iso.kbd
KEYMAPS+= uk.iso.kbd uk.cp850.kbd
KEYMAPS+= us.iso.kbd us.dvorak.kbd us.dvorakx.kbd us.unix.kbd

MAPSDIR = ${SHAREDIR}/syscons/keymaps
NOMAN = noman
NOOBJ = noobj

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${KEYMAPS} \
	    ${DESTDIR}${MAPSDIR}

.include <bsd.prog.mk>
