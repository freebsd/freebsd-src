# $FreeBSD: src/sys/modules/sound/driver/ich/Makefile,v 1.2.32.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_ich
SRCS		= device_if.h bus_if.h pci_if.h
SRCS		+= ich.c

.include <bsd.kmod.mk>
