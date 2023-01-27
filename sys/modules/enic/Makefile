#$FreeBSD: head/sys/modules/ix/Makefile 327031 2017-12-20 18:15:06Z erj $

.PATH:  ${SRCTOP}/sys/dev/enic

COPTS=-g
#WITH_DEBUG_FILES=yes
DEBUG_FLAGS=-g

KMOD    = if_enic
SRCS    = device_if.h bus_if.h pci_if.h pci_iov_if.h ifdi_if.h
SRCS	+= opt_inet.h opt_inet6.h opt_rss.h

SRCS	+= if_enic.c enic_txrx.c enic_res.c
SRCS	+= vnic_cq.c vnic_dev.c vnic_intr.c vnic_rq.c vnic_wq.c


CFLAGS+= -I${SRCTOP}/sys/dev/enic_native -I${SRCTOP}/sys/dev/enic_native/base

.include <bsd.kmod.mk>
