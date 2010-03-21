# $FreeBSD: src/sys/modules/sound/driver/ich/Makefile,v 1.2.36.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_ich
SRCS		= device_if.h bus_if.h pci_if.h
SRCS		+= ich.c

.include <bsd.kmod.mk>
