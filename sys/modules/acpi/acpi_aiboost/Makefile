# $FreeBSD: src/sys/modules/acpi/acpi_aiboost/Makefile,v 1.1 2006/10/30 05:38:48 takawata Exp $

.PATH:		${.CURDIR}/../../../dev/acpi_support

KMOD=		acpi_aiboost
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica
SRCS=		acpi_aiboost.c opt_acpi.h device_if.h bus_if.h acpi_if.h

.include <bsd.kmod.mk>
