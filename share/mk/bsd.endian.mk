# $FreeBSD$

.if ${MACHINE_ARCH} == "amd64" || \
    ${MACHINE_ARCH} == "i386" || \
    (${MACHINE} == "arm" && ${MACHINE_ARCH:Marm*eb*} == "") || \
    ${MACHINE_ARCH:Mmips*el} != ""
TARGET_ENDIANNESS= 1234
.elif ${MACHINE_ARCH} == "powerpc" || \
    ${MACHINE_ARCH} == "powerpc64" || \
    ${MACHINE_ARCH} == "sparc64" || \
    (${MACHINE} == "arm" && ${MACHINE_ARCH:Marm*eb*} != "") || \
    ${MACHINE_ARCH:Mmips*} != ""
TARGET_ENDIANNESS= 4321
.endif
