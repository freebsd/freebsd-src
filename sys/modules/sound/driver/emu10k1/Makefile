# $FreeBSD: src/sys/modules/sound/driver/emu10k1/Makefile,v 1.1.2.1 2000/09/23 19:15:02 cg Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_emu10k1
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= emu10k1.c

.include <bsd.kmod.mk>
