# $FreeBSD: src/sys/modules/sound/driver/sbc/Makefile,v 1.1.2.1 2000/09/23 19:15:06 cg Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/isa
KMOD		= snd_sbc
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= sbc.c

.include <bsd.kmod.mk>
