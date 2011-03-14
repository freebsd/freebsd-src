#
# $FreeBSD$
#

CXGBE = ${.CURDIR}/../../dev/cxgbe
.PATH: ${CXGBE} ${CXGBE}/common

KMOD = if_cxgbe
SRCS = t4_main.c t4_sge.c
SRCS+= t4_hw.c
SRCS+= device_if.h bus_if.h pci_if.h
SRCS+= opt_inet.h

CFLAGS+= -g -I${CXGBE}

.include <bsd.kmod.mk>
