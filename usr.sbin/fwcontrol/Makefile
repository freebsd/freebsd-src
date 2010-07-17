# $FreeBSD: src/usr.sbin/fwcontrol/Makefile,v 1.5.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	fwcontrol
SRCS=	fwcontrol.c fwcrom.c fwdv.c fwmpegts.c
MAN=	fwcontrol.8
WARNS=	3

.PATH:	${.CURDIR}/../../sys/dev/firewire

SDIR= ${.CURDIR}/../../sys
CFLAGS+=-I${.CURDIR} -I${SDIR}

.include <bsd.prog.mk>
