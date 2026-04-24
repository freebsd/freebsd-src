WORLDPACKAGE=	acpi

# On these platforms, acpi only contains config files.
.if ${MACHINE_ARCH} == "armv7" || ${MACHINE} == "powerpc" || \
    ${MACHINE_ARCH} == "riscv64"
SUBPACKAGES=
.endif

# ACPI isn't applicable in a jail.
PKG_SETS=	optional

.include <bsd.pkg.mk>
