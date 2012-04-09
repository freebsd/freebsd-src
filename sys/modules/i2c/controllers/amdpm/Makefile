# $FreeBSD: src/sys/modules/i2c/controllers/amdpm/Makefile,v 1.1.36.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.PATH:		${.CURDIR}/../../../../pci
KMOD		= amdpm
SRCS		= device_if.h bus_if.h pci_if.h smbus_if.h \
		  amdpm.c

.include <bsd.kmod.mk>
