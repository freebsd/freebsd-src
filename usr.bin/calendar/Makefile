#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	calendar
SRCS=   calendar.c io.c day.c ostern.c paskha.c
CFLAGS+= -Wall
INTER=          de_DE.ISO_8859-1 hr_HR.ISO_8859-2 ru_SU.KOI8-R
TEXTMODE?=	444

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${TEXTMODE} \
	    ${.CURDIR}/calendars/calendar.* ${DESTDIR}${SHAREDIR}/calendar
.for lang in ${INTER}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${TEXTMODE} \
		${.CURDIR}/calendars/${lang}/calendar.* \
		${DESTDIR}${SHAREDIR}/calendar/${lang}; 
.endfor

.include <bsd.prog.mk>
