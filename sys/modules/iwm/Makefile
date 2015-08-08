# $FreeBSD$

.PATH:  ${.CURDIR}/../../dev/iwm

KMOD=	if_iwm
# Main driver
SRCS=	if_iwm.c if_iwm_binding.c if_iwm_util.c if_iwm_phy_db.c
SRCS+=	if_iwm_mac_ctxt.c if_iwm_phy_ctxt.c if_iwm_time_event.c
SRCS+=	if_iwm_power.c if_iwm_scan.c
# bus layer
SRCS+=	if_iwm_pcie_trans.c

SRCS+=	device_if.h bus_if.h pci_if.h opt_wlan.h

CFLAGS+= -DIWM_DEBUG -I${.CURDIR}/../../dev/iwm

.include <bsd.kmod.mk>
