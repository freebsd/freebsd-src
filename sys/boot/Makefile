# $FreeBSD$

.if ${MACHINE_ARCH} == "amd64"
.MAKEFLAGS:  MACHINE_ARCH=i386 MACHINE=i386 REALLY_AMD64=true
.endif

.if !defined(NOFORTH)
# Build the add-in FORTH interpreter
SUBDIR+=		ficl
.endif

# Build OpenFirmware library
.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "sparc64"
SUBDIR+=		ofw
.endif

# Pick the machine-dependant subdir based on the target architecture.
SUBDIR+=		${MACHINE}

# Build ARC / AlphaBIOS executable on the Alpha
# (this is a WIP (work in progress))
#.if ${MACHINE_ARCH} == "alpha"
#SUBDIR+=		arc
#.endif

# Build EFI executable on ia64
.if ${MACHINE_ARCH} == "ia64"
SUBDIR+=		efi
.endif

.include <bsd.subdir.mk>
