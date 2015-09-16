# $FreeBSD$

# local configuration specific to meta mode
# XXX some of this should be in meta.sys.mk
# we assume that MK_META_MODE=yes

# we need this until there is an alternative
MK_INSTALL_AS_USER= yes

.if empty(OBJROOT) || ${.MAKE.LEVEL} == 0
.if !make(showconfig)
.if defined(MAKEOBJDIRPREFIX) && exists(${MAKEOBJDIRPREFIX})
.warning MAKEOBJDIRPREFIX not supported; setting MAKEOBJDIR...
# put things approximately where they want
OBJROOT:=${MAKEOBJDIRPREFIX}${SRCTOP:S,/src,,}/
MAKEOBJDIRPREFIX=
.export MAKEOBJDIRPREFIX
.endif
.if empty(MAKEOBJDIR) || ${MAKEOBJDIR:M*/*} == ""
# OBJTOP set below
MAKEOBJDIR=$${.CURDIR:S,$${SRCTOP},$${OBJTOP},}
# export but do not track
.export-env MAKEOBJDIR
# now for our own use
MAKEOBJDIR= ${.CURDIR:S,${SRCTOP},${OBJTOP},}
.endif
.endif
.if !empty(SB)
SB_OBJROOT ?= ${SB}/obj/
# this is what we use below
OBJROOT ?= ${SB_OBJROOT}
.endif
OBJROOT ?= ${SRCTOP:H}/obj/
.if ${OBJROOT:M*/} != ""
OBJROOT:= ${OBJROOT:tA}/
.else
OBJROOT:= ${OBJROOT:H:tA}/${OBJROOT:T}
.endif
.export OBJROOT SRCTOP

# we need HOST_TARGET etc below.
.include <host-target.mk>
.export HOST_TARGET
.endif

# from src/Makefile (for universe)
TARGET_ARCHES_arm?=     arm armeb armv6 armv6eb
TARGET_ARCHES_mips?=    mipsel mips mips64el mips64 mipsn32
TARGET_ARCHES_powerpc?= powerpc powerpc64
TARGET_ARCHES_pc98?=    i386

# some corner cases
BOOT_MACHINE_DIR.amd64 = boot/i386
MACHINE_ARCH.host = ${_HOST_ARCH}

# the list of machines we support
ALL_MACHINE_LIST?= amd64 arm i386 ia64 mips pc98 powerpc sparc64
.for m in ${ALL_MACHINE_LIST:O:u}
MACHINE_ARCH_LIST.$m?= ${TARGET_ARCHES_${m}:U$m}
MACHINE_ARCH.$m?= ${MACHINE_ARCH_LIST.$m:[1]}
BOOT_MACHINE_DIR.$m ?= boot/$m
.endfor

.ifndef _TARGET_SPEC
.if empty(MACHINE_ARCH)
.if !empty(TARGET_ARCH)
MACHINE_ARCH= ${TARGET_ARCH}
.else
MACHINE_ARCH= ${MACHINE_ARCH.${MACHINE}}
.endif
.endif
MACHINE_ARCH?= ${MACHINE_ARCH.${MACHINE}}
MACHINE_ARCH:= ${MACHINE_ARCH}
.else
# we got here via dirdeps
MACHINE_ARCH:= ${MACHINE_ARCH.${MACHINE}}
.endif

# now because for universe we want to potentially
# build for multiple MACHINE_ARCH per MACHINE
# we need more than MACHINE in TARGET_SPEC
TARGET_SPEC_VARS= MACHINE MACHINE_ARCH
# see dirdeps.mk
.if ${TARGET_SPEC:Uno:M*,*} != ""
_tspec := ${TARGET_SPEC:S/,/ /g}
MACHINE := ${_tspec:[1]}
MACHINE_ARCH := ${_tspec:[2]}
# etc.
# We need to stop that TARGET_SPEC affecting any submakes
# and deal with MACHINE=${TARGET_SPEC} in the environment.
TARGET_SPEC=
# export but do not track
.export-env TARGET_SPEC 
.export ${TARGET_SPEC_VARS}
.for v in ${TARGET_SPEC_VARS:O:u}
.if empty($v)
.undef $v
.endif
.endfor
.endif
# make sure we know what TARGET_SPEC is
# as we may need it to find Makefile.depend*
TARGET_SPEC = ${TARGET_SPEC_VARS:@v@${$v:U}@:ts,}

# to be consistent with src/Makefile just concatenate with '.'s
TARGET_OBJ_SPEC:= ${TARGET_SPEC:S;,;.;g}
OBJTOP:= ${OBJROOT}${TARGET_OBJ_SPEC}

.if ${.CURDIR} == ${SRCTOP}
RELDIR = .
.elif ${.CURDIR:M${SRCTOP}/*}
RELDIR := ${.CURDIR:S,${SRCTOP}/,,}
.endif

HOST_OBJTOP ?= ${OBJROOT}${HOST_TARGET}

.if ${OBJTOP} == ${HOST_OBJTOP} || ${REQUESTED_MACHINE:U${MACHINE}} == "host"
MACHINE= host
.if ${TARGET_MACHINE:Uno} == ${HOST_TARGET}
# not what we want
TARGET_MACHINE= host
.endif
.endif
.if ${MACHINE} == "host"
OBJTOP := ${HOST_OBJTOP}
.endif

.if ${.MAKE.LEVEL} == 0
PYTHON ?= /usr/local/bin/python
.export PYTHON
# this works best if share/mk is ready for it.
BUILD_AT_LEVEL0= no

# we want to end up with a singe stage tree for all machines
.if ${MK_STAGING} == "yes"
.if empty(STAGE_ROOT)
STAGE_ROOT?= ${OBJROOT}stage
.export STAGE_ROOT
.endif
.endif
.endif

.if ${MK_STAGING} == "yes"
.if ${MACHINE} == "host"
STAGE_MACHINE= ${HOST_TARGET}
.else
STAGE_MACHINE:= ${TARGET_OBJ_SPEC}
.endif
STAGE_OBJTOP:= ${STAGE_ROOT}/${STAGE_MACHINE}
STAGE_COMMON_OBJTOP:= ${STAGE_ROOT}/common
STAGE_HOST_OBJTOP:= ${STAGE_ROOT}/${HOST_TARGET}

STAGE_LIBDIR= ${STAGE_OBJTOP}${_LIBDIR:U${LIBDIR:U/lib}}
STAGE_INCLUDEDIR= ${STAGE_OBJTOP}${INCLUDEDIR:U/usr/include}
# this is not the same as INCLUDEDIR
STAGE_INCSDIR= ${STAGE_OBJTOP}${INCSDIR:U/include}
# the target is usually an absolute path
STAGE_SYMLINKS_DIR= ${STAGE_OBJTOP}

LDFLAGS_LAST+= -Wl,-rpath-link -Wl,${STAGE_LIBDIR}
.if ${MK_SYSROOT} == "yes"
SYSROOT?= ${STAGE_OBJTOP}/
.else
LDFLAGS_LAST+= -L${STAGE_LIBDIR}
.endif

.endif				# MK_STAGING

# this is sufficient for most of the tree.
.MAKE.DEPENDFILE_DEFAULT = ${.MAKE.DEPENDFILE_PREFIX}

# but if we have a machine qualified file it should be used in preference
.MAKE.DEPENDFILE_PREFERENCE = \
	${.MAKE.DEPENDFILE_PREFIX}.${MACHINE} \
	${.MAKE.DEPENDFILE_PREFIX}

.undef .MAKE.DEPENDFILE

.include "sys.dependfile.mk"

.if ${.MAKE.LEVEL} > 0 && ${MACHINE} == "host" && ${.MAKE.DEPENDFILE:E} != "host"
# we can use this but should not update it.
UPDATE_DEPENDFILE= NO
.endif

# define the list of places that contain files we are responsible for
.MAKE.META.BAILIWICK = ${SB} ${OBJROOT} ${STAGE_ROOT}

.if defined(CCACHE_DIR)
.MAKE.META.IGNORE_PATHS += ${CCACHE_DIR}
.endif

CSU_DIR.${MACHINE_ARCH} ?= csu/${MACHINE_ARCH}
CSU_DIR := ${CSU_DIR.${MACHINE_ARCH}}

.if !empty(TIME_STAMP)
TRACER= ${TIME_STAMP} ${:U}
.endif

# toolchains can be a pain - especially bootstrappping them
.if ${MACHINE} == "host"
MK_SHARED_TOOLCHAIN= no
.endif
.ifdef WITH_TOOLSDIR
TOOLSDIR?= ${HOST_OBJTOP}/tools
.elif defined(STAGE_HOST_OBJTOP) && exists(${STAGE_HOST_OBJTOP}/usr/bin)
TOOLSDIR?= ${STAGE_HOST_OBJTOP}
.endif
.if ${.MAKE.LEVEL} == 0 && exists(${TOOLSDIR}/usr/bin)
PATH:= ${PATH:S,:, ,g:@d@${exists(${TOOLSDIR}$d):?${TOOLSDIR}$d:}@:ts:}:${PATH}
.export PATH
.if exists(${TOOLSDIR}/usr/bin/cc)
HOST_CC?= ${TOOLSDIR}/usr/bin/cc
CC?= ${TOOLSDIR}/usr/bin/cc
CXX?= ${TOOLSDIR}/usr/bin/c++
.export HOST_CC CC CXX
.endif
.endif

.if ${MACHINE:Nhost:Ncommon} != "" && ${MACHINE} != ${HOST_MACHINE}
# cross-building
.if !defined(FREEBSD_REVISION)
FREEBSD_REVISION!= sed -n '/^REVISION=/{s,.*=,,;s,",,g;p; }' ${SRCTOP}/sys/conf/newvers.sh
.export FREEBSD_REVISION
.endif
CROSS_TARGET_FLAGS= -target ${MACHINE_ARCH}-unknown-freebsd${FREEBSD_REVISION}
CFLAGS+= ${CROSS_TARGET_FLAGS}
ACFLAGS+= ${CROSS_TARGET_FLAGS}
LDFLAGS+= -Wl,-m -Wl,elf_${MACHINE_ARCH}_fbsd
.endif
