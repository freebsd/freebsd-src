# $FreeBSD: src/sys/modules/sound/driver/ich/Makefile,v 1.2.34.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_ich
SRCS		= device_if.h bus_if.h pci_if.h
SRCS		+= ich.c

.include <bsd.kmod.mk>
