# $FreeBSD: src/usr.sbin/fwcontrol/Makefile,v 1.4 2003/04/30 03:39:25 simokawa Exp $

PROG=	fwcontrol
SRCS=	fwcontrol.c fwcrom.c fwdv.c
MAN=	fwcontrol.8

.PATH:	${.CURDIR}/../../sys/dev/firewire

SDIR= ${.CURDIR}/../../sys
CFLAGS+=-I${.CURDIR} -I${SDIR}

.include <bsd.prog.mk>
