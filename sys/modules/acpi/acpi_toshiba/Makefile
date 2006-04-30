# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_toshiba
WARNS?=	2
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_toshiba.c opt_acpi.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
