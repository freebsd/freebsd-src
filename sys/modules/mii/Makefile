# $FreeBSD: src/sys/modules/mii/Makefile,v 1.11.2.8 2003/10/29 17:06:48 wpaul Exp $

.PATH:	${.CURDIR}/../../dev/mii
KMOD	= miibus
SRCS	= mii.c mii_physubr.c ukphy.c ukphy_subr.c bus_if.h pci_if.h
SRCS	+= miibus_if.h device_if.h miibus_if.c e1000phy.c exphy.c nsphy.c
SRCS	+= mlphy.c tlphy.c rlphy.c amphy.c dcphy.c pnphy.c nsgphy.c
SRCS	+= pnaphy.c brgphy.c xmphy.c inphy.c lxtphy.c qsphy.c acphy.c
SRCS	+= ruephy.c tdkphy.c

.include <bsd.kmod.mk>
