# $FreeBSD: src/sys/modules/acpi/acpi_sony/Makefile,v 1.1.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/acpi_support

KMOD=		acpi_sony
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica
SRCS=		acpi_sony.c opt_acpi.h device_if.h bus_if.h acpi_if.h

.include <bsd.kmod.mk>
