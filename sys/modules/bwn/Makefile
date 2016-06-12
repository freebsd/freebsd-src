# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/bwn

KMOD=	if_bwn
SRCS=	if_bwn.c if_bwnreg.h if_bwnvar.h
SRCS+=	if_bwn_util.c

# PHY
SRCS+=	if_bwn_phy_common.c
SRCS+=	if_bwn_phy_g.c if_bwn_phy_lp.c
SRCS+=	if_bwn_phy_n.c

# Other
SRCS+=	device_if.h bus_if.h pci_if.h opt_bwn.h opt_wlan.h

# Uncomment this for the GPL PHY code; this requires the
# module be built with BWN_GPL_PHY set in the kernel
# configuration.

#.PATH: ${.CURDIR}/../../gnu/dev/bwn/phy_n
#SRCS+=  if_bwn_radio_2055.c
#SRCS+=  if_bwn_radio_2056.c
#SRCS+=  if_bwn_radio_2057.c
#SRCS+=  if_bwn_phy_n_tables.c
#SRCS+=  if_bwn_phy_n_ppr.c
#SRCS+=  if_bwn_phy_n_core.c

.include <bsd.kmod.mk>

# XXX Work around clang warning, until maintainer approves fix.
CWARNFLAGS.if_bwn.c=	${NO_WSOMETIMES_UNINITIALIZED}
CWARNFLAGS.if_bwn_phy_g.c=	${NO_WSOMETIMES_UNINITIALIZED}
CWARNFLAGS.if_bwn_phy_lp.c=	${NO_WSOMETIMES_UNINITIALIZED}
