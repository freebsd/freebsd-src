# $FreeBSD$

.PATH:  ${.CURDIR}/../../dev/e1000e
KMOD    = if_em
SRCS    = device_if.h bus_if.h pci_if.h opt_ddb.h opt_em.h opt_inet.h \
	  opt_inet6.h 
SRCS    += $(CORE_SRC) $(LEGACY_SRC)
SRCS	+= $(COMMON_SHARED) $(LEGACY_SHARED) $(PCIE_SHARED)
CORE_SRC = if_em.c e1000e_osdep.c
# This is the Legacy, pre-PCIE source, it can be
# undefined when using modular driver if not needed
COMMON_SHARED = e1000e_api.c e1000e_phy.c e1000e_nvm.c e1000e_mac.c \
		e1000e_manage.c e1000e_vf.c e1000e_mbx.c e1000e_80003es2lan.c
LEGACY_SHARED = e1000e_82540.c e1000e_82541.c e1000e_82542.c e1000e_82543.c \
		e1000e_82571.c
PCIE_SHARED = e1000e_ich8lan.c

CFLAGS += -I${.CURDIR}/../../dev/e1000e

# DEVICE_POLLING for a non-interrupt-driven method
#CFLAGS  += -DDEVICE_POLLING

.include <bsd.kmod.mk>
