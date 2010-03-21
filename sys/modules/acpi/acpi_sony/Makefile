# $FreeBSD: src/sys/modules/acpi/acpi_sony/Makefile,v 1.1.22.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/acpi_support

KMOD=		acpi_sony
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica
SRCS=		acpi_sony.c opt_acpi.h device_if.h bus_if.h acpi_if.h

.include <bsd.kmod.mk>
