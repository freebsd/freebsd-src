#$FreeBSD$

.PATH:  ${.CURDIR}/../../dev/i40e

KMOD    = if_i40e
SRCS    = device_if.h bus_if.h pci_if.h opt_bdg.h
SRCS    += if_i40e.c i40e_txrx.c i40e_osdep.c

# Shared source
SRCS    += i40e_common.c i40e_nvm.c i40e_adminq.c i40e_lan_hmc.c i40e_hmc.c

CFLAGS	+= -DSMP

# Add Flow Director support
# CFLAGS += -DI40E_FDIR
# Debug messages / sysctls
# CFLAGS += -DI40E_DEBUG

.include <bsd.kmod.mk>
