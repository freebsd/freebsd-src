# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/mii

KMOD=	miibus
SRCS=	acphy.c amphy.c atphy.c bmtphy.c brgphy.c bus_if.h ciphy.c device_if.h
SRCS+=	e1000phy.c exphy.c gentbi.c icsphy.c inphy.c ip1000phy.c jmphy.c
SRCS+=	lxtphy.c miibus_if.c miibus_if.h mii.c miidevs.h mii_physubr.c
SRCS+=	mlphy.c nsgphy.c nsphy.c nsphyter.c pci_if.h pnaphy.c qsphy.c
SRCS+=	rdcphy.c rgephy.c rlphy.c ruephy.c tdkphy.c tlphy.c truephy.c
SRCS+=	ukphy.c ukphy_subr.c
SRCS+=	xmphy.c

EXPORT_SYMS=	mii_attach	\
		mii_mediachg	\
		mii_phy_probe	\
		mii_phy_reset	\
		mii_pollstat	\
		mii_tick

.include <bsd.kmod.mk>
