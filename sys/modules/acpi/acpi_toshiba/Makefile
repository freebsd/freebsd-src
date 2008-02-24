# $FreeBSD: src/sys/modules/acpi/acpi_toshiba/Makefile,v 1.5 2006/05/30 09:38:53 ru Exp $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_toshiba
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_toshiba.c opt_acpi.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
