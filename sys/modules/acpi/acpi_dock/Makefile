# $FreeBSD: src/sys/modules/acpi/acpi_dock/Makefile,v 1.2.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/acpica
KMOD=		acpi_dock
SRCS=		acpi_dock.c opt_acpi.h device_if.h bus_if.h acpi_if.h
SRCS+=		opt_ddb.h

.include <bsd.kmod.mk>
