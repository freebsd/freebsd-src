# $FreeBSD: src/lib/libcalendar/Makefile,v 1.4.2.1 2001/04/25 10:04:20 ru Exp $

LIB=	calendar

SRCS=	calendar.c easter.c
INCS=	calendar.h

MAN=	calendar.3

MLINKS=	calendar.3 easterg.3 calendar.3 easterog.3 calendar.3 easteroj.3 \
	calendar.3 gdate.3 calendar.3 jdate.3 \
	calendar.3 ndaysg.3 calendar.3 ndaysj.3 \
	calendar.3 week.3 calendar.3 weekday.3

CFLAGS+=-I. -I${.CURDIR} -Wall

.include <bsd.lib.mk>
