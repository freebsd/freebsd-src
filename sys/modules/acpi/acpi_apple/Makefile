.PATH:	${SRCTOP}/sys/dev/acpi_apple

KMOD=	acpi_apple
SRCS=	acpi_apple.c acpi_apple_platform.c acpi_apple_bl.c \
	opt_acpi.h acpi_if.h bus_if.h device_if.h \
	pci_if.h backlight_if.h

.include <bsd.kmod.mk>
