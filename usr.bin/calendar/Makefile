#	@(#)Makefile	4.8 (Berkeley) 5/11/90

PROG=	calendar

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/calendars/calendar.* ${DESTDIR}/usr/share/calendar

.include <bsd.prog.mk>
