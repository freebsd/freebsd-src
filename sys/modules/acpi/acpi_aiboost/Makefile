# $FreeBSD: src/sys/modules/acpi/acpi_aiboost/Makefile,v 1.1.12.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/acpi_support

KMOD=		acpi_aiboost
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica
SRCS=		acpi_aiboost.c opt_acpi.h device_if.h bus_if.h acpi_if.h

.include <bsd.kmod.mk>
