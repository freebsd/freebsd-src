# $FreeBSD: src/sys/modules/sound/driver/ich/Makefile,v 1.2 2003/02/07 13:56:32 nyan Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_ich
SRCS		= device_if.h bus_if.h pci_if.h
SRCS		+= ich.c

.include <bsd.kmod.mk>
