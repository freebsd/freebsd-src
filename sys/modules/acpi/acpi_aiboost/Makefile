# $FreeBSD: src/sys/modules/acpi/acpi_aiboost/Makefile,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/acpi_support

KMOD=		acpi_aiboost
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica
SRCS=		acpi_aiboost.c opt_acpi.h device_if.h bus_if.h acpi_if.h

.include <bsd.kmod.mk>
