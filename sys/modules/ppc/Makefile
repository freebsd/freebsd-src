# $FreeBSD: src/sys/modules/ppc/Makefile,v 1.3.8.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/ppc
KMOD=	ppc
SRCS=	bus_if.h device_if.h ppbus_if.h isa_if.h pci_if.h serdev_if.h \
	opt_ppc.h opt_isa.h \
	ppc.c ppc_pci.c ppc_puc.c

.if ${MACHINE_ARCH} == "amd64"
SRCS+=	ppc_acpi.c ppc_isa.c
.endif

.if ${MACHINE_ARCH} == "i386"
.if ${MACHINE} != "pc98"
SRCS+=	ppc_acpi.c
.endif
SRCS+=	ppc_isa.c
.endif

.if ${MACHINE_ARCH} == "ia64"
SRCS+=	ppc_acpi.c
.endif

.include <bsd.kmod.mk>
