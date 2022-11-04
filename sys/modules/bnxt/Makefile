#$FreeBSD$

.PATH:  ${SRCTOP}/sys/dev/bnxt

KMOD    = if_bnxt
SRCS    = device_if.h bus_if.h pci_if.h pci_iov_if.h ifdi_if.h
SRCS	+= opt_inet.h opt_inet6.h opt_rss.h
SRCS    += bnxt_txrx.c if_bnxt.c
SRCS	+= bnxt_hwrm.c
SRCS	+= bnxt_sysctl.c
SRCS	+= bnxt_mgmt.c

CFLAGS+= -DIFLIB

.include <bsd.kmod.mk>
