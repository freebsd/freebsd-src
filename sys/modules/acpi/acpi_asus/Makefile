# $FreeBSD$

.PATH:		${.CURDIR}/../../../i386/acpica \
		${.CURDIR}/../../../dev/led

KMOD=		acpi_asus
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica
SRCS=		acpi_asus.c led.c opt_acpi.h device_if.h bus_if.h

.include <bsd.kmod.mk>
