# $FreeBSD$
#
# Author: Harti Brandt <harti@freebsd.org>
#
.PATH: ${.CURDIR}/../../dev/hatm

KMOD=	if_hatm
SRCS=	if_hatm.c if_hatm_intr.c if_hatm_ioctl.c if_hatm_tx.c if_hatm_rx.c \
	device_if.h bus_if.h pci_if.h opt_inet.h opt_natm.h

CFLAGS+= -DENABLE_BPF
# CFLAGS+= -DHATM_DEBUG -DINVARIANT_SUPPORT -DINVARIANTS -g
# LDFLAGS+= -g

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

opt_natm.h:
	echo "#define NATM 1" > opt_natm.h

.include <bsd.kmod.mk>
