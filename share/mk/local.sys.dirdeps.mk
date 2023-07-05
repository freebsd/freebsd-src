# $FreeBSD$

# local configuration specific to meta mode
# we assume that MK_DIRDEPS_BUILD=yes

# we need this until there is an alternative
MK_INSTALL_AS_USER= yes

.-include <site.sys.dirdeps.mk>
# previously only included for DIRDEPS_BUILD anyway
.-include <site.meta.sys.mk>

.if ${MK_STAGING} == "yes"

STAGE_TARGET_OBJTOP:= ${STAGE_ROOT}/${TARGET_OBJ_SPEC}
# These are exported for hooking in out-of-tree builds.  They will always
# be overridden in sub-makes above when building in-tree.
.if ${.MAKE.LEVEL} > 0
.export STAGE_OBJTOP STAGE_TARGET_OBJTOP STAGE_HOST_OBJTOP
.endif

# Use tools/install.sh which can avoid the need for xinstall for simple cases.
INSTALL?=	sh ${SRCTOP}/tools/install.sh
# This is for stage-install to pickup from the environment.
REAL_INSTALL:=	${INSTALL}
.export REAL_INSTALL
STAGE_INSTALL=	sh ${.PARSEDIR:tA}/stage-install.sh OBJDIR=${.OBJDIR:tA}

STAGE_LIBDIR= ${STAGE_OBJTOP}${_LIBDIR:U${LIBDIR:U/lib}}
STAGE_INCLUDEDIR= ${STAGE_OBJTOP}${INCLUDEDIR:U/usr/include}
# this is not the same as INCLUDEDIR
STAGE_INCSDIR= ${STAGE_OBJTOP}${INCSDIR:U/include}
# the target is usually an absolute path
STAGE_SYMLINKS_DIR= ${STAGE_OBJTOP}

#LDFLAGS_LAST+= -Wl,-rpath-link,${STAGE_LIBDIR}
.if ${MK_SYSROOT} == "yes"
SYSROOT?= ${STAGE_OBJTOP}
.else
LDFLAGS_LAST+= -L${STAGE_LIBDIR}
.endif

.endif				# MK_STAGING

.-include "local.toolchain.mk"

.if ${.MAKE.LEVEL} > 0 && ${MACHINE} == "host" && ${.MAKE.DEPENDFILE:E} != "host"
# we can use this but should not update it.
UPDATE_DEPENDFILE?= NO
.endif

# define the list of places that contain files we are responsible for
.MAKE.META.BAILIWICK = ${SB} ${OBJROOT} ${STAGE_ROOT}

CSU_DIR.${MACHINE_ARCH} ?= csu/${MACHINE_ARCH}
CSU_DIR := ${CSU_DIR.${MACHINE_ARCH}}

.if !empty(TIME_STAMP)
TRACER= ${TIME_STAMP} ${:U}
.endif
.if !defined(_RECURSING_PROGS) && !defined(_RECURSING_CRUNCH) && \
    !make(print-dir)
WITH_META_STATS= t
.endif

# toolchains can be a pain - especially bootstrappping them
.if ${MACHINE} == "host"
MK_SHARED_TOOLCHAIN= no
.endif
TOOLCHAIN_VARS=	AS AR CC CLANG_TBLGEN CXX CPP LD NM OBJCOPY RANLIB \
		STRINGS SIZE LLVM_TBLGEN
_toolchain_bin_CLANG_TBLGEN=	/usr/bin/clang-tblgen
_toolchain_bin_LLVM_TBLGEN=	/usr/bin/llvm-tblgen
_toolchain_bin_CXX=		/usr/bin/c++
.ifdef WITH_TOOLSDIR
TOOLSDIR?= ${HOST_OBJTOP}/tools
.elif defined(STAGE_HOST_OBJTOP)
TOOLSDIR?= ${STAGE_HOST_OBJTOP}
.endif
.if ${MK_DIRDEPS_BUILD} == "yes" && ${MACHINE} != "host"
# ideally tools needed by makefiles like sh,csh,tinfo
# would be built in their own directories but for now
# this works well enough.
BTOOLSPATH= ${HOST_OBJTOP}/${RELDIR}
.else
# Only define if it exists in case user didn't run bootstrap-tools.  Otherwise
# the tool will be built during the build.  Building it assumes it is
# TARGET==MACHINE.
.if exists(${HOST_OBJTOP}/tools${.CURDIR})
BTOOLSPATH= ${HOST_OBJTOP}/tools${.CURDIR}
.endif
.endif

# Don't use the bootstrap tools logic on itself.
.if ${.TARGETS:Mbootstrap-tools} == "" && \
    !make(test-system-*) && !make(showconfig) && !make(print-dir) && \
    !defined(BOOTSTRAPPING_TOOLS) && !empty(TOOLSDIR) && ${.MAKE.LEVEL} == 0
.for dir in /sbin /bin /usr/sbin /usr/bin
PATH:= ${TOOLSDIR}${dir}:${PATH}
.endfor
.export PATH
# Prefer the TOOLSDIR version of the toolchain if present vs the host version.
.for var in ${TOOLCHAIN_VARS}
_toolchain_bin.${var}=	${TOOLSDIR}${_toolchain_bin_${var}:U/usr/bin/${var:tl}}
.if exists(${_toolchain_bin.${var}})
HOST_${var}?=	${_toolchain_bin.${var}}
.export		HOST_${var}
.endif
.endfor
.endif

.for var in ${TOOLCHAIN_VARS}
HOST_${var}?=	${_toolchain_bin_${var}:U/usr/bin/${var:tl}}
.endfor

.if ${MACHINE} == "host"
.for var in ${TOOLCHAIN_VARS}
${var}=		${HOST_${var}}
.endfor
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
.endif

# we set these here, rather than local.gendirdeps.mk
# so we can ensure any DEP_* values that might be used in
# conditionals do not cause syntax errors when Makefile.depend
# is included at level 1+

# order of this list matters!
GENDIRDEPS_FILTER_DIR_VARS+= \
       CSU_DIR \
       BOOT_MACHINE_DIR

# order of this list matters!
GENDIRDEPS_FILTER_VARS+= \
       KERNEL_NAME \
       DEP_MACHINE_CPUARCH \
       DEP_MACHINE_ARCH \
       DEP_MACHINE

.if ${.MAKE.LEVEL} > 0
.for V in ${GENDIRDEPS_FILTER_DIR_VARS:MDEP_*:O:u} \
	${GENDIRDEPS_FILTER_VARS:MDEP_*:O:u}
$V?= ${${V:S,DEP_,,}}
.endfor
.endif

.if ${MACHINE} == "host" && ${.MAKE.OS} != "FreeBSD"
# some makefiles expect this
BOOTSTRAPPING= 0
.endif
