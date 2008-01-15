# $FreeBSD: src/sys/modules/acpi/Makefile,v 1.48.2.3 2007/10/11 23:47:28 jkim Exp $

.if ${MACHINE} == "i386"
SUBDIR=		acpi
.endif

SUBDIR+=	acpi_asus acpi_fujitsu acpi_ibm				\
		acpi_panasonic acpi_sony acpi_toshiba acpi_video	\
		acpi_dock

.include <bsd.subdir.mk>
