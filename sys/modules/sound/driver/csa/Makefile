# $FreeBSD: src/sys/modules/sound/driver/csa/Makefile,v 1.1.2.1 2000/09/23 19:15:01 cg Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_csa
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= csa.c

.include <bsd.kmod.mk>
