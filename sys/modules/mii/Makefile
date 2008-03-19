# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/mii

KMOD=	miibus
SRCS=	acphy.c amphy.c bmtphy.c brgphy.c bus_if.h ciphy.c dcphy.c device_if.h
SRCS+=	e1000phy.c exphy.c gentbi.c icsphy.c inphy.c ip1000phy.c
SRCS+=	lxtphy.c miibus_if.c miibus_if.h mii.c miidevs.h mii_physubr.c
SRCS+=	mlphy.c nsgphy.c nsphy.c nsphyter.c pci_if.h pnaphy.c pnphy.c qsphy.c
SRCS+=	rgephy.c rlphy.c ruephy.c tdkphy.c tlphy.c ukphy.c ukphy_subr.c
SRCS+=	xmphy.c

EXPORT_SYMS=	mii_mediachg	\
		mii_phy_probe	\
		mii_phy_reset	\
		mii_pollstat	\
		mii_tick

.include <bsd.kmod.mk>
