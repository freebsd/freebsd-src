# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_panasonic
WARNS?=	2
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_panasonic.c opt_acpi.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
