.PATH:  ${SRCTOP}/sys/dev/bnxt/bnxt_en

KMOD    = if_bnxt
SRCS    = device_if.h bus_if.h pci_if.h pci_iov_if.h ifdi_if.h
SRCS	+= opt_inet.h opt_inet6.h opt_rss.h opt_ratelimit.h opt_kern_tls.h
SRCS    += bnxt_txrx.c if_bnxt.c
SRCS	+= bnxt_hwrm.c
SRCS	+= bnxt_sysctl.c
SRCS	+= bnxt_mgmt.c
SRCS	+= bnxt_dcb.c bnxt_dcb.h
SRCS	+= bnxt_auxbus_compat.c bnxt_auxbus_compat.h
SRCS	+= bnxt_sriov.c bnxt_sriov.h
SRCS	+= bnxt_ulp.c bnxt_ulp.h
SRCS	+= bnxt_log.c bnxt_log.h
SRCS	+= bnxt_log_data.c bnxt_log_data.h
SRCS	+= bnxt_coredump.c bnxt_coredump.h
SRCS	+= bnxt_ptp.c bnxt_ptp.h
SRCS	+= bnxt_mpc.c bnxt_mpc.h
SRCS	+= bnxt_ktls.c bnxt_ktls.h
SRCS	+= bnxt_compat.h
SRCS	+= ${LINUXKPI_GENSRCS}

CFLAGS+= -DIFLIB
CFLAGS+= -I${SRCTOP}/sys/ofed/include
CFLAGS+= ${LINUXKPI_INCLUDES}

.include <bsd.kmod.mk>
