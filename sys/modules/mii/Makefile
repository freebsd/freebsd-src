
.PATH: ${SRCTOP}/sys/dev/mii

KMOD=	miibus
SRCS=	acphy.c amphy.c atphy.c axphy.c bmtphy.c brgphy.c
SRCS+=	ciphy.c dp83822phy.c dp83867phy.c
SRCS+=	e1000phy.c gentbi.c icsphy.c ip1000phy.c jmphy.c lxtphy.c
SRCS+=	miibus_if.c mii.c mii_bitbang.c mii_physubr.c
SRCS+=	nsgphy.c nsphy.c nsphyter.c pnaphy.c qsphy.c
SRCS+=	rdcphy.c rgephy.c rlphy.c smcphy.c
SRCS+=	smscphy.c tdkphy.c truephy.c
SRCS+=	ukphy.c ukphy_subr.c
SRCS+=	vscphy.c xmphy.c
SRCS+=	bus_if.h device_if.h miibus_if.h miidevs.h opt_platform.h pci_if.h

.if !empty(OPT_FDT)
SRCS+=	micphy.c mii_fdt.c
SRCS+=	ofw_bus_if.h
.endif

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
