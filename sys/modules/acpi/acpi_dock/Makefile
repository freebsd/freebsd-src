# $FreeBSD: src/sys/modules/acpi/acpi_dock/Makefile,v 1.1.2.1 2007/03/23 19:45:53 njl Exp $

.PATH:		${.CURDIR}/../../../dev/acpica
KMOD=		acpi_dock
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica
SRCS=		acpi_dock.c opt_acpi.h device_if.h bus_if.h acpi_if.h
SRCS+=		opt_ddb.h

.include <bsd.kmod.mk>
