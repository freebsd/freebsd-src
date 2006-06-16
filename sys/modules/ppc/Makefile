
# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/ppc
KMOD=	ppc
SRCS=	bus_if.h device_if.h ppbus_if.h isa_if.h pci_if.h serdev_if.h \
	opt_ppc.h opt_isa.h \
	ppc_acpi.c ppc_pci.c \
	ppc.c ppc_isa.c ppc_puc.c

.include <bsd.kmod.mk>
