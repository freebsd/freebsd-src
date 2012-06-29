#
# $FreeBSD$
#

CXGBE = ${.CURDIR}/../../../dev/cxgbe
.PATH: ${CXGBE}/tom

KMOD = t4_tom
SRCS = t4_tom.c t4_connect.c t4_listen.c t4_cpl_io.c t4_tom_l2t.c
SRCS+= device_if.h bus_if.h pci_if.h
SRCS+= opt_inet.h

CFLAGS+= -I${CXGBE}

.include <bsd.kmod.mk>
