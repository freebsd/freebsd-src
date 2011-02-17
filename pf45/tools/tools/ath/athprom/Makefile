# $FreeBSD$

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
