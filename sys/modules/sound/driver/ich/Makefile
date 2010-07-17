# $FreeBSD: src/sys/modules/sound/driver/ich/Makefile,v 1.2.34.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_ich
SRCS		= device_if.h bus_if.h pci_if.h
SRCS		+= ich.c

.include <bsd.kmod.mk>
