# $FreeBSD: src/usr.sbin/cxgbtool/Makefile,v 1.1.12.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	cxgbtool
SRCS=	cxgbtool.c
NO_MAN=
CFLAGS+= -I${.CURDIR}/../../sys/dev/cxgb -I.
CFLAGS+= -DCONFIG_T3_REGS -DCHELSIO_INTERNAL

install:

.include <bsd.prog.mk>