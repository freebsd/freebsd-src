#$FreeBSD$

.PATH:  ${.CURDIR}/../../dev/ixl

KMOD    = if_ixl
SRCS    = device_if.h bus_if.h pci_if.h pci_iov_if.h
SRCS    += opt_inet.h opt_inet6.h opt_rss.h
SRCS    += if_ixl.c ixl_txrx.c i40e_osdep.c

# Shared source
SRCS    += i40e_common.c i40e_nvm.c i40e_adminq.c i40e_lan_hmc.c i40e_hmc.c

CFLAGS	+= -DSMP -DIXL_DEBUG_SYSCTL

# Add Flow Director support
# CFLAGS += -DIXL_FDIR
# Debug messages / sysctls
# CFLAGS += -DIXLE_DEBUG

.include <bsd.kmod.mk>
