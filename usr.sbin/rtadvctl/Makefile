# $FreeBSD$
#
.PATH:	${.CURDIR}/../rtadvd

PROG=	rtadvctl
MAN=	rtadvctl.8

SRCS=	rtadvctl.c control.c control_client.c if.c timer_subr.c

CFLAGS+= -DROUTEINFO -I${.CURDIR} -I${.CURDIR}/../rtadvd
WARNS?=	3

.include <bsd.prog.mk>
