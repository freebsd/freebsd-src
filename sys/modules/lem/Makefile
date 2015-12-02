# $FreeBSD$

.PATH:  ${.CURDIR}/../../dev/e1000
KMOD    = if_lem
SRCS    = device_if.h bus_if.h pci_if.h opt_ddb.h opt_lem.h opt_inet.h \
	  opt_inet6.h 
SRCS    += $(CORE_SRC) $(LEGACY_SRC)
SRCS	+= $(COMMON_SHARED) $(LEGACY_SHARED) $(PCIE_SHARED)

CORE_SRC = if_lem.c e1000_osdep.c
COMMON_SHARED = e1000_api.c e1000_phy.c e1000_nvm.c e1000_mac.c \
		e1000_manage.c e1000_vf.c e1000_mbx.c

PCIE_SHARED = e1000_80003es2lan.c e1000_82571.c e1000_ich8lan.c
LEGACY_SHARED = e1000_82540.c e1000_82541.c e1000_82542.c e1000_82543.c

CFLAGS += -I${.CURDIR}/../../dev/e1000

# DEVICE_POLLING for a non-interrupt-driven method
#CFLAGS  += -DDEVICE_POLLING

.include <bsd.kmod.mk>
