# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/mii
KMOD	= miibus
SRCS	= mii.c mii_physubr.c ukphy.c ukphy_subr.c bus_if.h pci_if.h
SRCS	+= miibus_if.h device_if.h miibus_if.c exphy.c nsphy.c
SRCS	+= mlphy.c tlphy.c rlphy.c amphy.c dcphy.c pnphy.c
SRCS	+= brgphy.c xmphy.c pnaphy.c

.include <bsd.kmod.mk>
