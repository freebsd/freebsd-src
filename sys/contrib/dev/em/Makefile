#$FreeBSD$

.include <bsd.own.mk>

.PATH:  ${.CURDIR}

KMOD    = if_em
SRCS    = device_if.h bus_if.h pci_if.h opt_bdg.h
SRCS    += $(CORE_SRC) $(COMMON_SHARED) $(LEGACY_SHARED) $(PCIE_SHARED)
CORE_SRC = if_em.c if_lem.c e1000_osdep.c
# Shared
COMMON_SHARED = e1000_api.c e1000_phy.c e1000_nvm.c e1000_mac.c e1000_manage.c
PCIE_SHARED = e1000_80003es2lan.c e1000_ich8lan.c e1000_82571.c
LEGACY_SHARED = e1000_82540.c e1000_82542.c e1000_82541.c e1000_82543.c

# These flags are only used when in a standalone tarball build
CFLAGS += -DINET -DINET6 -DEM_STANDALONE_BUILD

# Uncomment this to disable Fast interrupt handling.
#CFLAGS  += -DEM_LEGACY_IRQ

# DEVICE_POLLING for a non-interrupt-driven method
#CFLAGS  += -DDEVICE_POLLING

# Uncomment this to enable the stack multiqueue routines
# with this driver you do not get multiple tx queues,
# but it does provide input queuing. Testing has shown
# some stability issues so its off by default.
# NOTE: it has been found that UDP intensive traffic
#       actually does better with the old stack interface
#       and so it seems better to have this off by default.
#       however it works fine, and some workloads may benefit
#       having it on.
#CFLAGS  += -DEM_MULTIQUEUE

clean:
	rm -f opt_bdg.h device_if.h bus_if.h pci_if.h setdef*
	rm -f *.o *.kld *.ko
	rm -f @ machine export_syms x86
	rm -f ${CLEANFILES}

.include <bsd.kmod.mk>
