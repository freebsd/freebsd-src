# $FreeBSD: src/tools/tools/cxgbtool/Makefile,v 1.1.4.2.2.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	cxgbtool
SRCS=	cxgbtool.c
NO_MAN=
CFLAGS+= -I${.CURDIR}/../../../sys/dev/cxgb -I.
CFLAGS+= -DCONFIG_T3_REGS -DCHELSIO_INTERNAL
BINDIR?= /usr/sbin

.include <bsd.prog.mk>
