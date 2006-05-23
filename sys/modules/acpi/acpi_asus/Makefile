# $FreeBSD: src/sys/modules/acpi/acpi_asus/Makefile,v 1.4 2004/08/03 21:27:42 njl Exp $

.PATH:	${.CURDIR}/../../../i386/acpica

KMOD=	acpi_asus
WARNS?=	2
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_asus.c opt_acpi.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
