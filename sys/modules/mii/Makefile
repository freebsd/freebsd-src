# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/mii

KMOD=	miibus
SRCS=	mii.c mii_physubr.c ukphy.c ukphy_subr.c bus_if.h pci_if.h
SRCS+=	miibus_if.h miidevs.h device_if.h miibus_if.c e1000phy.c exphy.c nsphy.c
SRCS+=	mlphy.c tlphy.c rlphy.c amphy.c dcphy.c pnphy.c inphy.c tdkphy.c
SRCS+=	bmtphy.c brgphy.c xmphy.c pnaphy.c lxtphy.c qsphy.c acphy.c nsgphy.c
SRCS+=	rgephy.c ruephy.c
SRCS+=	opt_carp.h

EXPORT_SYMS=	mii_mediachg	\
		mii_tick	\
		mii_pollstat	\
		mii_phy_probe	\
		mii_phy_reset

.include <bsd.kmod.mk>
