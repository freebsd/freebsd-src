#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.include <src.opts.mk>

PROG=	calendar
SRCS=	calendar.c locale.c events.c dates.c parsedata.c io.c day.c \
	ostern.c paskha.c pom.c sunpos.c
LIBADD=	m
INTER=          de_AT.ISO_8859-15 de_DE.ISO8859-1 fr_FR.ISO8859-1 \
		hr_HR.ISO8859-2 hu_HU.ISO8859-2 pt_BR.ISO8859-1 \
		pt_BR.UTF-8 ru_RU.KOI8-R ru_RU.UTF-8 uk_UA.KOI8-U
DE_LINKS=       de_DE.ISO8859-15
FR_LINKS=       fr_FR.ISO8859-15
TEXTMODE?=	444

beforeinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${TEXTMODE} \
	    ${.CURDIR}/calendars/calendar.* ${DESTDIR}${SHAREDIR}/calendar
.for lang in ${INTER}
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

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
