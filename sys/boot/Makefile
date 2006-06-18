# $FreeBSD$

.include <bsd.own.mk>

.if ${MK_FORTH} != "no"
# Build the add-in FORTH interpreter.
SUBDIR+=		ficl
.endif

# Build EFI library.
.if ${MACHINE_ARCH} == "ia64"
SUBDIR+=		efi
.endif

# Build Open Firmware library.
.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "sparc64"
SUBDIR+=		ofw
.endif

.if ${MACHINE_ARCH} != "arm"
# Pick the machine-dependent subdir based on the target architecture.
SUBDIR+=		${MACHINE:S/amd64/i386/}
.endif

.include <bsd.subdir.mk>
