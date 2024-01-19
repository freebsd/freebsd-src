
.if ${MACHINE_CPUARCH} == "aarch64" || \
    ${MACHINE_CPUARCH} == "arm" || \
    ${MACHINE_ARCH} == "amd64" || \
    ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "powerpc64le" || \
    ${MACHINE_CPUARCH} == "riscv"
TARGET_ENDIANNESS= 1234
CAP_MKDB_ENDIAN= -l
LOCALEDEF_ENDIAN= -l
.elif ${MACHINE_ARCH} == "powerpc" || \
    ${MACHINE_ARCH} == "powerpc64" || \
    ${MACHINE_ARCH} == "powerpcspe"
TARGET_ENDIANNESS= 4321
CAP_MKDB_ENDIAN= -b
LOCALEDEF_ENDIAN= -b
.elif ${.MAKE.OS} == "FreeBSD"
.error Don't know the endian of this architecture
.else
#
# During bootstrapping on !FreeBSD OSes, we need to define some value.  Short of
# having an exhaustive list for all variants of Linux and MacOS we simply do not
# set TARGET_ENDIANNESS and poison the other variables. They should be unused
# during the bootstrap phases (apart from one place that's adequately protected
# in bsd.compiler.mk) where we're building the bootstrap tools.
#
.if !defined(TARGET_ENDIANNESS)
TARGET_ENDIANESS!=echo "__BYTE_ORDER__" | ${CC} -E -P -
.if ${TARGET_ENDIANNESS} == "1234"
CAP_MKDB_ENDIAN= -l
LOCALEDEF_ENDIAN= -l
.elif ${TARGET_ENDIANNESS} == "4321"
CAP_MKDB_ENDIAN= -b
LOCALEDEF_ENDIAN= -b
.else
.error Don't know the endian of this architecture
.endif
.endif
.endif
