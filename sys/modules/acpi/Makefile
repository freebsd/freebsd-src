# $FreeBSD$

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "ia64"
.error "ACPI can only be compiled into the kernel on the amd64 and ia64 platforms"
.endif

.if ${MACHINE} != "i386"
.error "The ACPI module is only for i386"
.endif

SUBDIR= acpi acpi_asus acpi_fujitsu acpi_ibm acpi_panasonic acpi_sony \
	acpi_toshiba acpi_video

.include <bsd.subdir.mk>
