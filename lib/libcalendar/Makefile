#	$Id: Makefile,v 1.1.1.1 1997/12/04 10:41:49 helbig Exp $

LIB=	calendar

SRCS=	calendar.c easter.c

MAN3=   calendar.3

MLINKS=	calendar.3 easterg.3 calendar.3 easterog.3 calendar.3 easteroj.3 \
	calendar.3 gdate.3 calendar.3 jdate.3 \
	calendar.3 ndaysg.3 calendar.3 ndaysj.3 \
	calendar.3 week.3 calendar.3 weekday.3

CFLAGS+=-I. -I${.CURDIR} -Wall

beforeinstall:
	${INSTALL} -C -m 444 -o $(BINOWN) -g $(BINGRP) ${.CURDIR}/calendar.h \
		${DESTDIR}/usr/include

.include <bsd.lib.mk>
