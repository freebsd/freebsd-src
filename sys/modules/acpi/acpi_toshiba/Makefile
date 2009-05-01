# $FreeBSD: src/sys/modules/acpi/acpi_toshiba/Makefile,v 1.5.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_toshiba
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_toshiba.c opt_acpi.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
