# $FreeBSD: src/sys/modules/sound/driver/es1888/Makefile,v 1.1.2.1 2000/09/23 19:15:02 cg Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/isa
KMOD		= snd_es1888
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= es1888.c

.include <bsd.kmod.mk>
