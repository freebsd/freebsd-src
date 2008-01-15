# $FreeBSD: src/usr.sbin/cxgbtool/Makefile,v 1.1.2.1 2007/03/15 03:21:32 kmacy Exp $

PROG=	cxgbtool
SRCS=	cxgbtool.c
NO_MAN=
CFLAGS+= -I${.CURDIR}/../../sys/dev/cxgb -I.
CFLAGS+= -DCONFIG_T3_REGS -DCHELSIO_INTERNAL

install:

.include <bsd.prog.mk>