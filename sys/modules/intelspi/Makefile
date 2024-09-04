.PATH:	${SRCTOP}/sys/dev/intel
KMOD=	intelspi
SRCS=	spi.c spi_acpi.c spi_pci.c
SRCS+=	acpi_if.h pci_if.h device_if.h bus_if.h opt_acpi.h opt_pci.h spibus_if.h

.include <bsd.kmod.mk>
