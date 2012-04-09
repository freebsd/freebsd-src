# $FreeBSD: src/share/mk/bsd.endian.mk,v 1.5.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.if ${MACHINE_ARCH} == "amd64" || \
    ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "ia64" || \
    (${MACHINE_ARCH} == "arm" && !defined(TARGET_BIG_ENDIAN)) || \
    (${MACHINE_ARCH} == "mips" && !defined(TARGET_BIG_ENDIAN))
TARGET_ENDIANNESS= 1234
.elif ${MACHINE_ARCH} == "powerpc" || \
    ${MACHINE_ARCH} == "sparc64" || \
    ${MACHINE_ARCH} == "arm" || \
    ${MACHINE_ARCH} == "mips"
TARGET_ENDIANNESS= 4321
.endif
