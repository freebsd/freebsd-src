# $FreeBSD: src/sys/modules/hifn/Makefile,v 1.2 2002/11/13 17:45:42 mux Exp $

.PATH:	${.CURDIR}/../../dev/hifn
KMOD	= hifn
SRCS	= hifn7751.c
SRCS   += device_if.h bus_if.h pci_if.h
SRCS   += opt_bdg.h opt_bus.h crypto_if.h

.include <bsd.kmod.mk>
