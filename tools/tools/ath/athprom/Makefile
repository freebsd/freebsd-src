# $FreeBSD: src/tools/tools/ath/athprom/Makefile,v 1.1.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

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
