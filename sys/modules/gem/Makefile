# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/gem

KMOD=	if_gem
SRCS=	bus_if.h device_if.h if_gem.c if_gem_pci.c miibus_if.h
SRCS+=	${ofw_bus_if} pci_if.h opt_platform.h

.if ${MACHINE_CPUARCH} == "powerpc"
ofw_bus_if=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
