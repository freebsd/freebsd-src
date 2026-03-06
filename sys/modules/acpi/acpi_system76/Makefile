.PATH:	${SRCTOP}/sys/dev/acpi_support

KMOD=	acpi_system76
CFLAGS+=-I${SRCTOP}/sys/dev/acpi_support
SRCS=	acpi_system76.c opt_acpi.h acpi_if.h device_if.h bus_if.h

.include <bsd.kmod.mk>
