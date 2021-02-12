#$FreeBSD$

.PATH:  ${SRCTOP}/sys/dev/iavf

KMOD    = if_iavf
SRCS    = device_if.h bus_if.h pci_if.h ifdi_if.h
SRCS    += opt_inet.h opt_inet6.h opt_rss.h opt_iflib.h
SRCS    += if_iavf_iflib.c iavf_lib.c iavf_osdep.c iavf_txrx_iflib.c
SRCS    += iavf_vc_common.c iavf_vc_iflib.c

# Shared source
SRCS    += iavf_adminq.c iavf_common.c

# Debug messages / sysctls
# CFLAGS += -DIAVF_DEBUG
# Enable asserts and other debugging facilities
# CFLAGS += -DINVARIANTS -DINVARIANTS_SUPPORT -DWITNESS

SYMLINKS=	${KMOD}.ko ${KMODDIR}/if_ixlv.ko

.include <bsd.kmod.mk>
