# $FreeBSD$

.if !defined(NOFORTH)
# Build the add-in FORTH interpreter.
SUBDIR+=		ficl
.endif

# Build Open Firmware library.
.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "sparc64"
SUBDIR+=		ofw
.endif

# Pick the machine-dependent subdir based on the target architecture.
SUBDIR+=		${MACHINE:S/amd64/i386/}

# Build ARC / AlphaBIOS executable on the Alpha
# (this is a WIP (work in progress)).
#.if ${MACHINE_ARCH} == "alpha"
#SUBDIR+=		arc
#.endif

# Build EFI executable on ia64.
.if ${MACHINE_ARCH} == "ia64"
SUBDIR+=		efi
.endif

.include <bsd.subdir.mk>
