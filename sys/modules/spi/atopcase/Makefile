.PATH: ${SRCTOP}/sys/dev/atopcase
KMOD=	atopcase
SRCS=	atopcase.c atopcase_acpi.c
SRCS+=	acpi_if.h backlight_if.h bus_if.h device_if.h hid_if.h opt_acpi.h \
	opt_hid.h opt_spi.h spibus_if.h usbdevs.h

.include <bsd.kmod.mk>
