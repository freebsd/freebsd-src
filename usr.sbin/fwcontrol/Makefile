# $FreeBSD$

PROG=	fwcontrol
SRCS=	fwcontrol.c fwcrom.c fwdv.c
MAN=	fwcontrol.8

.PATH:	${.CURDIR}/../../sys/dev/firewire

SDIR= ${.CURDIR}/../../sys
CFLAGS+=-I${.CURDIR} -I${SDIR}

.include <bsd.prog.mk>
