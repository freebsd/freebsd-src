
# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/ppc
KMOD=	ppc
SRCS=	bus_if.h device_if.h ppbus_if.h isa_if.h pci_if.h serdev_if.h \
	opt_ppc.h opt_isa.h \
	ppc_pci.c \
	ppc.c ppc_isa.c ppc_puc.c

.if ${MACHINE} != "pc98"
SRCS+=	ppc_acpi.c
.endif

.include <bsd.kmod.mk>
