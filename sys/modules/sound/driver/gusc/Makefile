# $FreeBSD: src/sys/modules/sound/driver/gusc/Makefile,v 1.1.2.1 2000/09/23 19:15:03 cg Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/isa
KMOD		= snd_gusc
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= gusc.c

.include <bsd.kmod.mk>
