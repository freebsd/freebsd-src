# $FreeBSD$

# Build the add-in FORTH interpreter
SUBDIR+=		ficl

# Build OpenFirmware library on PowerPC
.if ${MACHINE_ARCH} == "powerpc"
SUBDIR+=		ofw
.endif

# Pick the machine-dependant subdir based on the target architecture.
# XXX pc98 is basicly a special i386 with MACHINE_ARCH=i386 and MACHINE=pc98.
.if ${MACHINE_ARCH} == "i386" && ${MACHINE} == "pc98"
SUBDIR+=		${MACHINE}
.else
SUBDIR+=		${MACHINE_ARCH}
.endif

# Build ARC / AlphaBIOS executable on the Alpha
.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=		arc
.endif

.include <bsd.subdir.mk>
