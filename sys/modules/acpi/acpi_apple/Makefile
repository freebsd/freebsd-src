.PATH:	${SRCTOP}/sys/dev/apple_acpi

KMOD=	acpi_apple
SRCS=	apple_acpi.c apple_platform.c apple_backlight.c \
	opt_acpi.h acpi_if.h bus_if.h device_if.h \
	pci_if.h backlight_if.h

.include <bsd.kmod.mk>
