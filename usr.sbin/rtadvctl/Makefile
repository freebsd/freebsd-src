# $FreeBSD: src/usr.sbin/rtadvctl/Makefile,v 1.1.4.2.2.1 2012/03/03 06:15:13 kensmith Exp $
#
.PATH:	${.CURDIR}/../rtadvd

PROG=	rtadvctl
MAN=	rtadvctl.8

SRCS=	rtadvctl.c control.c control_client.c if.c timer_subr.c

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../rtadvd
WARNS?=	1

.include <bsd.prog.mk>
