# $FreeBSD: src/sys/boot/Makefile,v 1.32.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

.if ${MK_FORTH} != "no"
# Build the add-in FORTH interpreter.
SUBDIR+=		ficl
.endif

# Build EFI library.
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "ia64"
SUBDIR+=		efi
.endif

# Build Open Firmware library.
.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "sparc64"
SUBDIR+=		ofw
.endif

# Pick the machine-dependent subdir based on the target architecture.
SUBDIR+=		${MACHINE:S/amd64/i386/:S/sun4v/sparc64/}

.include <bsd.subdir.mk>
