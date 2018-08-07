#$FreeBSD$

.PATH:  ${SRCTOP}/sys/dev/ixl

KMOD    = if_ixlv
SRCS    = device_if.h bus_if.h pci_if.h ifdi_if.h
SRCS    += opt_inet.h opt_inet6.h opt_rss.h opt_ixl.h opt_iflib.h
SRCS    += if_ixlv.c ixlvc.c ixl_txrx.c i40e_osdep.c

# Shared source
SRCS    += i40e_common.c i40e_nvm.c i40e_adminq.c

# Debug messages / sysctls
# CFLAGS += -DIXL_DEBUG

.include <bsd.kmod.mk>
