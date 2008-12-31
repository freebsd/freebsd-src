# $FreeBSD: src/sys/modules/sound/driver/ich/Makefile,v 1.2.30.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_ich
SRCS		= device_if.h bus_if.h pci_if.h
SRCS		+= ich.c

.include <bsd.kmod.mk>
