#$FreeBSD$

.PATH:  ${SRCTOP}/sys/dev/ice

KMOD    = if_ice
SRCS    = device_if.h bus_if.h pci_if.h ifdi_if.h
SRCS    += opt_inet.h opt_inet6.h opt_rss.h opt_iflib.h
SRCS    += ice_lib.c ice_osdep.c ice_resmgr.c ice_strings.c
SRCS    += ice_iflib_recovery_txrx.c ice_iflib_txrx.c if_ice_iflib.c

# Shared source
SRCS    += ice_common.c ice_controlq.c ice_dcb.c ice_flex_pipe.c ice_flow.c
SRCS    += ice_nvm.c ice_sched.c ice_sriov.c ice_switch.c

.include <bsd.kmod.mk>
