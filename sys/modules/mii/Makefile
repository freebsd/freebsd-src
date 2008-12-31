# $FreeBSD: src/sys/modules/mii/Makefile,v 1.30.2.5.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/mii

KMOD=	miibus
SRCS=	acphy.c amphy.c atphy.c bmtphy.c brgphy.c bus_if.h ciphy.c device_if.h
SRCS+=	e1000phy.c exphy.c gentbi.c icsphy.c inphy.c ip1000phy.c jmphy.c
SRCS+=	lxtphy.c miibus_if.c miibus_if.h mii.c miidevs.h mii_physubr.c
SRCS+=	mlphy.c nsgphy.c nsphy.c nsphyter.c pci_if.h pnaphy.c qsphy.c
SRCS+=	rgephy.c rlphy.c ruephy.c tdkphy.c tlphy.c truephy.c ukphy.c
SRCS+=	ukphy_subr.c
SRCS+=	xmphy.c

EXPORT_SYMS=	mii_mediachg	\
		mii_phy_probe	\
		mii_phy_reset	\
		mii_pollstat	\
		mii_tick

.include <bsd.kmod.mk>
