.PATH:  ${SRCTOP}/sys/dev/enic

KMOD    = if_enic
SRCS    = device_if.h bus_if.h pci_if.h pci_iov_if.h ifdi_if.h
SRCS	+= opt_inet.h opt_inet6.h opt_rss.h

SRCS	+= if_enic.c enic_txrx.c enic_res.c
SRCS	+= vnic_cq.c vnic_dev.c vnic_intr.c vnic_rq.c vnic_wq.c

.include <bsd.kmod.mk>
