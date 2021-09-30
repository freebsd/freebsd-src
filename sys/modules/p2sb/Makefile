.PATH: ${.CURDIR}/../../dev/p2sb

KMOD= p2sb
SRCS= p2sb.c lewisburg_gpio.c lewisburg_gpiocm.c
SRCS+=	device_if.h bus_if.h pci_if.h gpio_if.h opt_platform.h

.include <bsd.kmod.mk>
