
# For universe we want to potentially
# build for multiple MACHINE_ARCH per MACHINE
# so we need more than MACHINE in TARGET_SPEC
TARGET_SPEC_VARS?= MACHINE MACHINE_ARCH
 
# this is sufficient for most of the tree.
.MAKE.DEPENDFILE_DEFAULT = ${.MAKE.DEPENDFILE_PREFIX}

# but if we have a machine qualified file it should be used in preference
.MAKE.DEPENDFILE_PREFERENCE = \
	${.MAKE.DEPENDFILE_PREFIX}.${MACHINE} \
	${.MAKE.DEPENDFILE_PREFIX}

# before we process TARGET_SPEC
# we assume that MK_DIRDEPS_BUILD=yes

# from src/Makefile (for universe)
# would be nice to have this sort of info in sys.machine.mk
TARGET_ARCHES_arm?=     armv6 armv7
TARGET_ARCHES_arm64?=   aarch64
TARGET_ARCHES_powerpc?= powerpc powerpc64 powerpc64le powerpcspe
TARGET_ARCHES_riscv?=   riscv64

# some corner cases
BOOT_MACHINE_DIR.amd64 = boot/i386
MACHINE_ARCH.host = ${_HOST_ARCH}

# the list of machines we support
ALL_MACHINE_LIST?= amd64 arm arm64 i386 powerpc riscv

.-include <site.sys.dirdeps.env.mk>

.for m in ${ALL_MACHINE_LIST:O:u}
MACHINE_ARCH_LIST.$m?= ${TARGET_ARCHES_${m}:U$m}
MACHINE_ARCH.$m?= ${MACHINE_ARCH_LIST.$m:[1]}
BOOT_MACHINE_DIR.$m ?= boot/$m
.endfor

.if empty(MACHINE_ARCH)
.if !empty(TARGET_ARCH)
MACHINE_ARCH= ${TARGET_ARCH}
.else
MACHINE_ARCH= ${MACHINE_ARCH.${MACHINE}}
.endif
.endif
MACHINE_ARCH?= ${MACHINE_ARCH.${MACHINE}}
MACHINE_ARCH:= ${MACHINE_ARCH}

HOST_OBJTOP ?= ${OBJROOT}${HOST_TARGET}

.if ${REQUESTED_MACHINE:U${MACHINE}} == "host"
MACHINE= host
.if ${TARGET_MACHINE:Uno} == ${HOST_TARGET}
# not what we want
TARGET_MACHINE= host
.endif
.endif
.if ${MACHINE} == "host"
OBJTOP := ${HOST_OBJTOP}
MACHINE_ARCH= ${MACHINE_ARCH.${MACHINE}}
.endif


.if ${.MAKE.LEVEL} == 0 || empty(PYTHON)
PYTHON ?= /usr/local/bin/python
.export PYTHON

# _SKIP_BUILD is not 100% as it requires wrapping all 'all:' targets to avoid
# building in MAKELEVEL0.  Just prohibit 'all' entirely in this case to avoid
# problems.
.if make(all)
.error DIRDEPS_BUILD: Please run '${MAKE}' instead of '${MAKE} all'.
.endif
.endif

.if ${.MAKE.OS} != "FreeBSD" || ${_HOST_OSREL:R} < ${OS_REVISION:R}
# a pseudo option to indicate we need libegacy for host
MK_host_egacy= yes
.endif
