# $FreeBSD: src/sys/modules/glxsb/Makefile,v 1.1.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/glxsb
KMOD	= glxsb
SRCS	= glxsb.c glxsb_hash.c glxsb.h 
SRCS   += device_if.h pci_if.h bus_if.h
SRCS   += opt_bus.h cryptodev_if.h

.include <bsd.kmod.mk>
