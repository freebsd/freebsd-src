# $Id: Makefile,v 1.1.1.1 1995/08/05 23:23:41 ache Exp $

LOCALES=        ru_SU.KOI8-R
LOCALEDIR=      ${DESTDIR}/usr/share/locale

afterinstall:
	for l in ${LOCALES}; do \
		grep -v '^#' < ${.CURDIR}/data/$$l > ${LOCALEDIR}/$$l/LC_TIME; \
		chown ${BINOWN}.${BINGRP} ${LOCALEDIR}/$$l/LC_TIME; \
		chmod 644 ${LOCALEDIR}/$$l/LC_TIME; \
	done

.include <bsd.prog.mk>
