#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	calendar
SRCS=   calendar.c io.c day.c ostern.c
INTER=		de_DE.ISO_8859-1 hr_HR.ISO_8859-2
SHAREDIR=	/usr/share/calendar
TEXTMODE?=	444

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${TEXTMODE} \
	    ${.CURDIR}/calendars/calendar.* ${DESTDIR}${SHAREDIR}
.for lang in ${INTER}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${TEXTMODE} \
    		${.CURDIR}/calendars/${lang}/calendar.* \
		${DESTDIR}${SHAREDIR}/${lang}; 
.endfor

.include <bsd.prog.mk>
