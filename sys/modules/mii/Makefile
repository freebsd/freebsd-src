# $FreeBSD: src/sys/modules/mii/Makefile,v 1.11.2.2 2000/10/03 18:46:55 wpaul Exp $

.PATH:	${.CURDIR}/../../dev/mii
KMOD	= miibus
SRCS	= mii.c mii_physubr.c ukphy.c ukphy_subr.c bus_if.h pci_if.h
SRCS	+= miibus_if.h device_if.h miibus_if.c exphy.c nsphy.c
SRCS	+= mlphy.c tlphy.c rlphy.c amphy.c dcphy.c pnphy.c
SRCS	+= pnaphy.c brgphy.c xmphy.c

.include <bsd.kmod.mk>
