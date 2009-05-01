# $FreeBSD: src/usr.sbin/cxgbtool/Makefile,v 1.1.10.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	cxgbtool
SRCS=	cxgbtool.c
NO_MAN=
CFLAGS+= -I${.CURDIR}/../../sys/dev/cxgb -I.
CFLAGS+= -DCONFIG_T3_REGS -DCHELSIO_INTERNAL

install:

.include <bsd.prog.mk>