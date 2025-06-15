.PATH:  ${SRCTOP}/sys/dev/ice

KMOD    = if_ice

# Interface headers
SRCS    = device_if.h bus_if.h pci_if.h ifdi_if.h
SRCS	+= irdma_di_if.h irdma_if.h

# Option headers
SRCS    += opt_inet.h opt_inet6.h opt_rss.h opt_iflib.h

# Core source
SRCS    += ice_lib.c ice_osdep.c ice_resmgr.c ice_strings.c
SRCS    += ice_iflib_recovery_txrx.c ice_iflib_txrx.c if_ice_iflib.c
SRCS	+= ice_fw_logging.c ice_ddp_common.c
SRCS.PCI_IOV += pci_iov_if.h ice_iov.c ice_vf_mbx.c

# RDMA Client interface
# TODO: Is this the right way to compile this?
SRCS    += ice_rdma.c irdma_di_if.c irdma_if.c
CFLAGS.irdma_di_if.c += -I${SRCTOP}/sys/dev/ice
CFLAGS.irdma_if.c += -I${SRCTOP}/sys/dev/ice

# Shared source
SRCS    += ice_common.c ice_controlq.c ice_dcb.c ice_flex_pipe.c ice_flow.c
SRCS    += ice_nvm.c ice_sched.c ice_switch.c ice_vlan_mode.c ice_fwlog.c

.include <bsd.kmod.mk>
