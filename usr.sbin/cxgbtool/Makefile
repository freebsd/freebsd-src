# $FreeBSD: src/usr.sbin/cxgbtool/Makefile,v 1.1.2.1.4.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	cxgbtool
SRCS=	cxgbtool.c
NO_MAN=
CFLAGS+= -I${.CURDIR}/../../sys/dev/cxgb -I.
CFLAGS+= -DCONFIG_T3_REGS -DCHELSIO_INTERNAL

install:

.include <bsd.prog.mk>