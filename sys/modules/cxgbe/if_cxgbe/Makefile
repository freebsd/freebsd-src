#
# $FreeBSD: src/sys/modules/cxgbe/if_cxgbe/Makefile,v 1.1.2.4.2.1 2012/03/03 06:15:13 kensmith Exp $
#

CXGBE = ${.CURDIR}/../../../dev/cxgbe
.PATH: ${CXGBE} ${CXGBE}/common

KMOD = if_cxgbe
SRCS = t4_main.c t4_sge.c t4_l2t.c
SRCS+= t4_hw.c
SRCS+= device_if.h bus_if.h pci_if.h
SRCS+= opt_inet.h

CFLAGS+= -I${CXGBE}

# Provide the timestamp of a packet in its header mbuf.
#CFLAGS+= -DT4_PKT_TIMESTAMP

.include <bsd.kmod.mk>
