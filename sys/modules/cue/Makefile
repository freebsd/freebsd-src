# $FreeBSD: src/sys/modules/cue/Makefile,v 1.5 2001/01/06 13:59:43 obrien Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_cue
SRCS=	if_cue.c opt_bdg.h opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
