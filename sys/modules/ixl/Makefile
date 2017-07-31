#$FreeBSD$

.PATH:  ${SRCTOP}/sys/dev/ixl

KMOD    = if_ixl
SRCS    = device_if.h bus_if.h pci_if.h
SRCS    += opt_inet.h opt_inet6.h opt_rss.h opt_ixl.h
SRCS    += if_ixl.c ixl_pf_main.c ixl_pf_qmgr.c ixl_txrx.c ixl_pf_i2c.c i40e_osdep.c
SRCS	+= ixl_iw.c
SRCS.PCI_IOV=	pci_iov_if.h ixl_pf_iov.c

# Shared source
SRCS    += i40e_common.c i40e_nvm.c i40e_adminq.c i40e_lan_hmc.c i40e_hmc.c

# Debug messages / sysctls
# CFLAGS += -DIXL_DEBUG

.include <bsd.kmod.mk>
