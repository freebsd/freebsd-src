.PATH: ${SRCTOP}/sys/dev/rge

KMOD=	if_rge
SRCS=	if_rge.c if_rge_hw.c if_rge_stats.c if_rge_sysctl.c
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
