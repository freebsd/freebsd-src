# $FreeBSD: src/usr.sbin/cxgbtool/Makefile,v 1.1.16.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	cxgbtool
SRCS=	cxgbtool.c
NO_MAN=
CFLAGS+= -I${.CURDIR}/../../sys/dev/cxgb -I.
CFLAGS+= -DCONFIG_T3_REGS -DCHELSIO_INTERNAL

install:

.include <bsd.prog.mk>