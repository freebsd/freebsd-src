#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	calendar
SRCS=   calendar.c io.c day.c ostern.c paskha.c
INTER=          de_DE.ISO8859-1 hr_HR.ISO8859-2 ru_RU.KOI8-R
DE_LINKS=       de_DE.ISO_8859-1 de_DE.ISO8859-15 de_DE.ISO_8859-15
HR_LINKS=       hr_HR.ISO_8859-2
TEXTMODE?=	444

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${TEXTMODE} \
	    ${.CURDIR}/calendars/calendar.* ${DESTDIR}${SHAREDIR}/calendar
.for lang in ${INTER}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${TEXTMODE} \
		${.CURDIR}/calendars/${lang}/calendar.* \
		${DESTDIR}${SHAREDIR}/calendar/${lang}; 
.endfor
.for link in ${DE_LINKS}
	rm -rf ${DESTDIR}${SHAREDIR}/calendar/${link}
	ln -s de_DE.ISO8859-1 ${DESTDIR}${SHAREDIR}/calendar/${link}
.endfor
.for link in ${HR_LINKS}
	rm -rf ${DESTDIR}${SHAREDIR}/calendar/${link}
	ln -s hr_HR.ISO8859-2 ${DESTDIR}${SHAREDIR}/calendar/${link}
.endfor

.include <bsd.prog.mk>
