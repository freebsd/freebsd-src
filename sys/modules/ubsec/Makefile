# $FreeBSD: src/sys/modules/ubsec/Makefile,v 1.2.2.1 2002/11/21 23:38:47 sam Exp $

.PATH:	${.CURDIR}/../../dev/ubsec
KMOD	= ubsec
SRCS	= ubsec.c
SRCS   += device_if.h bus_if.h pci_if.h
SRCS   += opt_bdg.h opt_bus.h crypto_if.h

.include <bsd.kmod.mk>
