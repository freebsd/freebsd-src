# $FreeBSD$

.if ${MACHINE_ARCH} == "alpha" || \
    ${MACHINE_ARCH} == "amd64" || \
    ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "ia64"
TARGET_ENDIANNESS= 1234
.elif ${MACHINE_ARCH} == "powerpc" || \
    ${MACHINE_ARCH} == "sparc64"
TARGET_ENDIANNESS= 4321
.endif
