# $FreeBSD: src/sys/modules/acpi/acpi_dock/Makefile,v 1.1.8.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/acpica
KMOD=		acpi_dock
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica
SRCS=		acpi_dock.c opt_acpi.h device_if.h bus_if.h acpi_if.h
SRCS+=		opt_ddb.h

.include <bsd.kmod.mk>
