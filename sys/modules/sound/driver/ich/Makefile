# $FreeBSD: src/sys/modules/sound/driver/ich/Makefile,v 1.1 2001/07/01 19:38:58 cg Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_ich
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= ich.c

.include <bsd.kmod.mk>
