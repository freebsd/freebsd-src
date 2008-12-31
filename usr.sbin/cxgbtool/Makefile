# $FreeBSD: src/usr.sbin/cxgbtool/Makefile,v 1.1.8.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	cxgbtool
SRCS=	cxgbtool.c
NO_MAN=
CFLAGS+= -I${.CURDIR}/../../sys/dev/cxgb -I.
CFLAGS+= -DCONFIG_T3_REGS -DCHELSIO_INTERNAL

install:

.include <bsd.prog.mk>