#	$FreeBSD: src/sys/modules/acpi/acpi_video/Makefile,v 1.7 2007/06/24 20:36:51 njl Exp $

.PATH:	${.CURDIR}/../../../dev/acpica

KMOD=	acpi_video
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_video.c
SRCS+=	opt_acpi.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
