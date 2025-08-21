.PATH:	${SRCTOP}/sys/dev/gpio/

KMOD=	gpioaei
SRCS=	gpioaei.c

SRCS+=	\
	acpi_if.h \
	bus_if.h \
	device_if.h \
	gpio_if.h \
	gpiobus_if.h

SRCS+=	opt_acpi.h opt_platform.h

CFLAGS+=  -I. -I${SRCTOP}/sys/dev/gpio/

.include <bsd.kmod.mk>
