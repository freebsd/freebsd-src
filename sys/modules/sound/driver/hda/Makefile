# $FreeBSD: src/sys/modules/sound/driver/hda/Makefile,v 1.1.8.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci/hda

KMOD=	snd_hda
SRCS=	device_if.h bus_if.h pci_if.h channel_if.h mixer_if.h
SRCS+=	hdac.c hdac_private.h hdac_reg.h hda_reg.h hdac.h

.include <bsd.kmod.mk>
