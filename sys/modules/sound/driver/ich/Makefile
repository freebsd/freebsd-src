# $FreeBSD: src/sys/modules/sound/driver/ich/Makefile,v 1.2.28.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_ich
SRCS		= device_if.h bus_if.h pci_if.h
SRCS		+= ich.c

.include <bsd.kmod.mk>
