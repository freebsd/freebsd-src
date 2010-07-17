# $FreeBSD: src/sys/modules/acpi/acpi_wmi/Makefile,v 1.1.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_wmi
CFLAGS+=-I${.CURDIR}/../../../dev/acpi_support
SRCS=	acpi_wmi.c opt_acpi.h acpi_if.h acpi_wmi_if.h device_if.h bus_if.h

.include <bsd.kmod.mk>
