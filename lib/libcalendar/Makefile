# $FreeBSD: src/lib/libcalendar/Makefile,v 1.4 2000/01/14 07:57:23 rgrimes Exp $

LIB=	calendar

SRCS=	calendar.c easter.c
INCS=	calendar.h

MAN3=   calendar.3

MLINKS=	calendar.3 easterg.3 calendar.3 easterog.3 calendar.3 easteroj.3 \
	calendar.3 gdate.3 calendar.3 jdate.3 \
	calendar.3 ndaysg.3 calendar.3 ndaysj.3 \
	calendar.3 week.3 calendar.3 weekday.3

CFLAGS+=-I. -I${.CURDIR} -Wall

.include <bsd.lib.mk>
