.PATH:	${SRCTOP}/sys/dev/puc ${SRCTOP}/sys/dev/uart/

KMOD=	puc
SRCS=	puc.c puc_cfg.c puc_pci.c uart_bus_puc.c
SRCS+=	bus_if.h device_if.h serdev_if.c serdev_if.h pci_if.h

.include <bsd.kmod.mk>
