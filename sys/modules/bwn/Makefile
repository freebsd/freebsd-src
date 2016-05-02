# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/bwn

KMOD=	if_bwn
SRCS=	if_bwn.c if_bwnreg.h if_bwnvar.h
SRCS+=	if_bwn_phy_g.c if_bwn_phy_lp.c
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>

# XXX Work around clang warning, until maintainer approves fix.
CWARNFLAGS.if_bwn.c=	${NO_WSOMETIMES_UNINITIALIZED}
CWARNFLAGS.if_bwn_phy_g.c=	${NO_WSOMETIMES_UNINITIALIZED}
CWARNFLAGS.if_bwn_phy_lp.c=	${NO_WSOMETIMES_UNINITIALIZED}
