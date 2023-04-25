# $FreeBSD$

# local configuration specific to meta mode
# before we process TARGET_SPEC
# we assume that MK_DIRDEPS_BUILD=yes

.if !defined(HOST_TARGET) || !defined(HOST_MACHINE)
# we need HOST_TARGET etc below.
.include <host-target.mk>
.export HOST_TARGET
.endif

# from src/Makefile (for universe)
TARGET_ARCHES_arm?=     arm armv6 armv7
TARGET_ARCHES_arm64?=   aarch64
TARGET_ARCHES_powerpc?= powerpc powerpc64 powerpc64le powerpcspe
TARGET_ARCHES_riscv?=   riscv64

# some corner cases
BOOT_MACHINE_DIR.amd64 = boot/i386
MACHINE_ARCH.host = ${_HOST_ARCH}

# the list of machines we support
ALL_MACHINE_LIST?= amd64 arm arm64 i386 powerpc riscv

.-include <site.meta.sys.env.mk>

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

# For universe we want to potentially
# build for multiple MACHINE_ARCH per MACHINE
# so we need more than MACHINE in TARGET_SPEC
TARGET_SPEC_VARS?= MACHINE MACHINE_ARCH

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
.if ${MK_DIRDEPS_BUILD} == "yes" && ${.MAKE.LEVEL} == 0
.MAIN: dirdeps
.if make(all)
.error DIRDEPS_BUILD: Please run '${MAKE}' instead of '${MAKE} all'.
.endif
.endif
.endif

# this is sufficient for most of the tree.
.MAKE.DEPENDFILE_DEFAULT = ${.MAKE.DEPENDFILE_PREFIX}

# but if we have a machine qualified file it should be used in preference
.MAKE.DEPENDFILE_PREFERENCE = \
	${.MAKE.DEPENDFILE_PREFIX}.${MACHINE} \
	${.MAKE.DEPENDFILE_PREFIX}

.undef .MAKE.DEPENDFILE

META_MODE+=	missing-meta=yes
.if empty(META_MODE:Mnofilemon)
META_MODE+=	missing-filemon=yes
.endif

.if make(showconfig)
# this does not need/want filemon
UPDATE_DEPENDFILE= NO
.endif

.if ${MK_DIRDEPS_BUILD} == "yes"
.if ${.MAKE.OS} != "FreeBSD" || ${_HOST_OSREL:R} < ${OS_REVISION:R}
# a pseudo option to indicate we need libegacy for host
MK_host_egacy= yes
.endif
.endif
