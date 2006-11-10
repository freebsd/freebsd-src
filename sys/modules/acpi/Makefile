# $FreeBSD$

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "ia64"
.error "ACPI must be compiled in (not built as a module) on amd64 and ia64"
.endif

.if ${MACHINE} != "i386"
.error "The ACPI module is only for i386"
.endif

SUBDIR= acpi acpi_aiboost acpi_asus acpi_fujitsu acpi_ibm  \
	acpi_panasonic acpi_sony acpi_toshiba acpi_video acpi_dock

.include <bsd.subdir.mk>
