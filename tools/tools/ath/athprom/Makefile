# $FreeBSD: src/tools/tools/ath/athprom/Makefile,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	athprom

.include <../Makefile.inc>

TEMPLATEDIR=	/usr/local/libdata/athprom
TEXTMODE?=	444

CFLAGS+=-DDIR_TEMPLATE='"${TEMPLATEDIR}"'

beforeinstall:
	mkdir -p ${DESTDIR}${TEMPLATEDIR}
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${TEXTMODE} \
	    ${.CURDIR}/eeprom-* ${DESTDIR}${TEMPLATEDIR}/

.include <bsd.prog.mk>
