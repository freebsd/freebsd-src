.PATH:  ${SRCTOP}/sys/dev/bnxt/bnxt_en

KMOD    = if_bnxt
SRCS    = device_if.h bus_if.h pci_if.h pci_iov_if.h ifdi_if.h
SRCS	+= opt_inet.h opt_inet6.h opt_rss.h
SRCS    += bnxt_txrx.c if_bnxt.c
SRCS	+= bnxt_hwrm.c
SRCS	+= bnxt_sysctl.c
SRCS	+= bnxt_mgmt.c
SRCS	+= bnxt_dcb.c bnxt_dcb.h
SRCS	+= bnxt_auxbus_compat.c bnxt_auxbus_compat.h
SRCS	+= bnxt_ulp.c bnxt_ulp.h
SRCS	+= ${LINUXKPI_GENSRCS}

CFLAGS+= -DIFLIB
CFLAGS+= -I${SRCTOP}/sys/ofed/include
CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/common/include

.include <bsd.kmod.mk>
