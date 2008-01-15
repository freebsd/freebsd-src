#	$FreeBSD: src/sys/modules/acpi/acpi_video/Makefile,v 1.4.2.1 2007/06/28 21:23:56 njl Exp $

.PATH:	${.CURDIR}/../../../dev/acpica

KMOD=	acpi_video
WARNS?=	2
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_video.c
SRCS+=	opt_acpi.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
