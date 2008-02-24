#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/calendar/Makefile,v 1.31 2005/10/28 21:25:27 ru Exp $

PROG=	calendar
SRCS=   calendar.c io.c day.c ostern.c paskha.c
INTER=          de_AT.ISO_8859-15 de_DE.ISO8859-1 fr_FR.ISO8859-1 \
		hr_HR.ISO8859-2 hu_HU.ISO8859-2 ru_RU.KOI8-R uk_UA.KOI8-U
DE_LINKS=       de_DE.ISO8859-15
FR_LINKS=       fr_FR.ISO8859-15
TEXTMODE?=	444

beforeinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${TEXTMODE} \
	    ${.CURDIR}/calendars/calendar.* ${DESTDIR}${SHAREDIR}/calendar
.for lang in ${INTER}
	mkdir -p ${DESTDIR}${SHAREDIR}/calendar/${lang}
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${TEXTMODE} \
		${.CURDIR}/calendars/${lang}/calendar.* \
		${DESTDIR}${SHAREDIR}/calendar/${lang} 
.endfor
.for link in ${DE_LINKS}
	rm -rf ${DESTDIR}${SHAREDIR}/calendar/${link}
	ln -s de_DE.ISO8859-1 ${DESTDIR}${SHAREDIR}/calendar/${link}
.endfor
.for link in ${FR_LINKS}
	rm -rf ${DESTDIR}${SHAREDIR}/calendar/${link}
	ln -s fr_FR.ISO8859-1 ${DESTDIR}${SHAREDIR}/calendar/${link}
.endfor

.include <bsd.prog.mk>
