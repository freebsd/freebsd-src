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

# The following need the BWN_GPL_PHY kenrel option to opt-in
# to the GPL'd 802.11n PHY support for this driver.
.PATH: ${.CURDIR}/../../gnu/dev/bwn/phy_n
SRCS.BWN_GPL_PHY+=  if_bwn_radio_2055.c
SRCS.BWN_GPL_PHY+=  if_bwn_radio_2056.c
SRCS.BWN_GPL_PHY+=  if_bwn_radio_2057.c
SRCS.BWN_GPL_PHY+=  if_bwn_phy_n_tables.c
SRCS.BWN_GPL_PHY+=  if_bwn_phy_n_ppr.c
SRCS.BWN_GPL_PHY+=  if_bwn_phy_n_core.c

.include <bsd.kmod.mk>

# XXX Work around clang warnings, until maintainer approves fix.
CWARNFLAGS.if_bwn.c=	${NO_WSOMETIMES_UNINITIALIZED}
CWARNFLAGS.if_bwn_phy_g.c=	${NO_WSOMETIMES_UNINITIALIZED} ${NO_WCONSTANT_CONVERSION}
CWARNFLAGS.if_bwn_phy_lp.c=	${NO_WSOMETIMES_UNINITIALIZED}
