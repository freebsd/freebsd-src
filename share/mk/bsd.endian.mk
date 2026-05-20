
MACHINE_ARCH_LIST.little = \
	aarch64 \
	amd64 \
	armv7 \
	i386 \
	powerpc64le \
	riscv*

MACHINE_ARCH_LIST.big = \
	powerpc \
	powerpc64

.for e in big little
N_$e:= ${MACHINE_ARCH_LIST.$e:@m@N$m@:ts:}
.endfor

# For the host, we need to look at the host architecture
.if ${MACHINE:Nhost*} == ""
_ENDIAN_ARCH=${_HOST_ARCH}
.else
_ENDIAN_ARCH=${MACHINE_ARCH}
.endif

.if ${_ENDIAN_ARCH:${N_little}} == ""
TARGET_ENDIANNESS= 1234
CAP_MKDB_ENDIAN= -l
LOCALEDEF_ENDIAN= -l
.elif ${_ENDIAN_ARCH:${N_big}} == ""
TARGET_ENDIANNESS= 4321
CAP_MKDB_ENDIAN= -b
LOCALEDEF_ENDIAN= -b
.else
#
# During bootstrapping on !FreeBSD OSes, we need to define some value.  Short of
# having an exhaustive list for all variants of Linux and MacOS we simply do not
# set TARGET_ENDIANNESS (on Linux) and poison the other variables. They should
# be unused during the bootstrap phases (apart from one place that's adequately
# protected in bsd.compiler.mk) where we're building the bootstrap tools.
#
.if ${.MAKE.OS} == "Darwin"
# We do assume the endianness on macOS because Apple's modern hardware is all
# little-endian.  This might need revisited in the far future, but for the time
# being Apple Silicon's reign of terror continues.  We only set this one up
# because libcrypto is now built in bootstrap.
TARGET_ENDIANNESS= 1234
.endif
CAP_MKDB_ENDIAN= -B	# Poisoned value, invalid flags for both cap_mkdb
LOCALEDEF_ENDIAN= -B	# and localedef.
.endif

.if empty(TARGET_ENDIANNESS) && ${.MAKE.OS} == "FreeBSD"
.error Don't know the endianness of this architecture
.endif
