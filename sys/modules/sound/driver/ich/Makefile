# $FreeBSD: src/sys/modules/sound/driver/ich/Makefile,v 1.2.38.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_ich
SRCS		= device_if.h bus_if.h pci_if.h
SRCS		+= ich.c

.include <bsd.kmod.mk>
