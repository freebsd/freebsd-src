WITH_INSTALL_AS_USER= yes

.if defined(.PARSEDIR)		# bmake
.if !defined(_TARGETS)
# some things we do only once
_TARGETS := ${.TARGETS}
.export _TARGETS
.endif
.if ${_TARGETS:Mbuildworld}
WITHOUT_STAGING=
WITHOUT_SYSROOT=
UPDATE_DEPENDFILE=NO
NO_AUTO_OBJ=
.endif
SRCCONF:= ${.PARSEDIR}/src.conf
# ensure we are self contained
__MAKE_CONF:= ${SRCCONF}
.-include "src.conf"

# some handy macros
_this = ${.PARSEDIR:tA}/${.PARSEFILE}
# some useful modifiers

# A useful trick for testing multiple :M's against something
# :L says to use the variable's name as its value - ie. literal
# got = ${clean* destroy:${M_ListToMatch:S,V,.TARGETS,}}
M_ListToMatch = L:@m@$${V:M$$m}@
# match against our initial targets (see above)
M_L_TARGETS = ${M_ListToMatch:S,V,_TARGETS,}

# turn a list into a set of :N modifiers
# NskipFoo = ${Foo:${M_ListToSkip}}
M_ListToSkip= O:u:ts::S,:,:N,g:S,^,N,

# type should be a builtin in any sh since about 1980,
# AUTOCONF := ${autoconf:L:${M_whence}}
M_type = @x@(type $$x 2> /dev/null); echo;@:sh:[0]:N* found*:[@]:C,[()],,g
M_whence = ${M_type}:M/*:[1]

# convert a path to a valid shell variable
M_P2V = tu:C,[./-],_,g

# absoulte path to what we are reading.
_PARSEDIR = ${.PARSEDIR:tA}

.if !empty(SB)
SB_SRC ?= ${SB}/src
SB_OBJROOT ?= ${SB}/obj/
# this is what we use below
SRCTOP ?= ${SB_SRC}
OBJROOT ?= ${SB_OBJROOT}
.endif

.if empty(SRCTOP)
SRCTOP := ${_PARSEDIR:H:H}
OBJROOT ?= ${SRCTOP:H}/obj/
OBJROOT := ${OBJROOT}
.endif
.export OBJROOT SRCTOP

# we need HOST_TARGET etc below.
.include <host-target.mk>

# from src/Makefile (for universe)
TARGET_ARCHES_arm?=     arm armeb armv6 armv6eb
TARGET_ARCHES_mips?=    mipsel mips mips64el mips64 mipsn32
TARGET_ARCHES_powerpc?= powerpc powerpc64
TARGET_ARCHES_pc98?=    i386

# some corner cases
CSU_DIR.i386 = csu/i386-elf
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

.if ${.MAKE.LEVEL} == 0
# 1st time only
.-include <sys.env.mk>
.if !empty(OBJROOT) 
.if ${OBJROOT:M*/} != ""
OBJROOT:= ${OBJROOT:tA}/
.else
OBJROOT:= ${OBJROOT:H:tA}/${OBJROOT:T}
.endif
.export OBJROOT
.endif
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

.if !empty(SRCTOP)
.if ${.CURDIR} == ${SRCTOP}
RELDIR = .
.elif ${.CURDIR:M${SRCTOP}/*}
RELDIR := ${.CURDIR:S,${SRCTOP}/,,}
.endif
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

# the logic in bsd.own.mk forces this dance
.ifndef WITHOUT_META_MODE
WITH_META_MODE= yes

.ifndef WITHOUT_SYSROOT
WITH_SYSROOT= yes
.endif
.ifndef WITHOUT_STAGING
WITH_STAGING= yes
.ifndef WITHOUT_STAGING_PROG
WITH_STAGING_PROG= yes
.endif
.endif

PYTHON ?= /usr/local/bin/python

.if ${.MAKE.LEVEL} == 0
# just in case -m, MAKESYSPATH or our default has .../ 
# export a sanitised version...
# first any -m* from command line,
# then any MAKESYSPATH and finally ${.PARSEDIR}
_makesyspath:= ${.MAKEFLAGS:tW:S/ -m / -m/g:tw:M-m*:S,^-m,,} \
	${MAKESYSPATH:U} \
	${.PARSEDIR}
# replace .../.* with ${.PARSEDIR}, not perfect but pretty close
MAKESYSPATH:= ${_makesyspath:S,:, ,g:C,^\.\.\./.*,${.PARSEDIR},:u:ts:}
.export MAKESYSPATH

# this works best if share/mk is ready for it.
BUILD_AT_LEVEL0= no
# By default only MACHINE0 updates dependencies
# see local.autodep.mk
MACHINE0 := ${MACHINE}
.export MACHINE0
.export PYTHON
.endif

# we want to end up with a singe stage tree for all machines
.ifndef WITHOUT_STAGING
.if empty(STAGE_ROOT)
STAGE_ROOT?= ${OBJROOT}stage
.export STAGE_ROOT
.endif
.endif

.if !empty(STAGE_ROOT) && !defined(WITHOUT_STAGING)
.if ${MACHINE} == "host"
STAGE_MACHINE= ${HOST_TARGET}
.else
STAGE_MACHINE:= ${TARGET_OBJ_SPEC}
.endif
STAGE_OBJTOP:= ${STAGE_ROOT}/${STAGE_MACHINE}
STAGE_COMMON_OBJTOP:= ${STAGE_ROOT}/common
STAGE_HOST_OBJTOP:= ${STAGE_ROOT}/${HOST_TARGET}

STAGE_LIBDIR= ${STAGE_OBJTOP}${_LIBDIR:U${LIBDIR:U/lib}}
# this is not the same as INCLUDEDIR
STAGE_INCSDIR= ${STAGE_OBJTOP}${INCSDIR:U/include}
# the target is usually an absolute path
STAGE_SYMLINKS_DIR= ${STAGE_OBJTOP}

.if ${MACHINE} == "host" && defined(EARLY_BUILD)
# we literally want to build with host cc and includes
.else
.ifdef WITH_SYSROOT
SYSROOT?= ${STAGE_OBJTOP}/
.endif
LDFLAGS_LAST+= -Wl,-rpath-link -Wl,${STAGE_LIBDIR}
STAGED_INCLUDE_DIR= ${STAGE_OBJTOP}/usr/include
.endif
.endif				# EARLY_BUILD for host

# this is sufficient for most of the tree.
.MAKE.DEPENDFILE_DEFAULT = ${.MAKE.DEPENDFILE_PREFIX}

# but if we have a machine qualified file it should be used in preference
.MAKE.DEPENDFILE_PREFERENCE = \
	${.MAKE.DEPENDFILE_PREFIX}.${MACHINE} \
	${.MAKE.DEPENDFILE_PREFIX}

.undef .MAKE.DEPENDFILE

.include "sys.dependfile.mk"

.include "meta.sys.mk"

.if ${.MAKE.LEVEL} > 0 && ${MACHINE} == "host" && ${.MAKE.DEPENDFILE:E} != "host"
# we can use this but should not update it.
UPDATE_DEPENDFILE= NO
.endif

.MAKE.META.BAILIWICK = ${SB} ${OBJROOT} ${STAGE_ROOT}

CSU_DIR.i386 = csu/i386-elf
CSU_DIR.${MACHINE_ARCH} ?= csu/${MACHINE_ARCH}
CSU_DIR := ${CSU_DIR.${MACHINE_ARCH}}

.endif				# meta mode

# ensure we have a value
.MAKE.MODE ?= normal

MAKE_PRINT_VAR_ON_ERROR+= \
	.CURDIR \
	.MAKE \
	.OBJDIR \
	.TARGETS \
	DESTDIR \
	LD_LIBRARY_PATH \
	MACHINE \
	MACHINE_ARCH \
	MAKEOBJDIRPREFIX \
	MAKESYSPATH \
	MAKE_VERSION\
	OBJTOP \
	${MAKE_PRINT_VAR_ON_ERROR_XTRAS}

.if ${.MAKE.LEVEL} > 0
MAKE_PRINT_VAR_ON_ERROR += .MAKE.MAKEFILES .PATH
.endif


# these are handy
# we can use this for a cheap timestamp at the start of a target's script,
# but not at the end - since make will expand both at the same time.
AnEmptyVar=
TIME_STAMP_FMT = @ %s [%Y-%m-%d %T]
TIME_STAMP = ${TIME_STAMP_FMT:localtime}
# this will produce the same output but as of when date(1) is run.
TIME_STAMP_DATE = `date '+${TIME_STAMP_FMT}'`
TIME_STAMP_END?= ${TIME_STAMP_DATE}

.ifdef WITH_TIMESTAMPS
TRACER= ${TIME_STAMP} ${AnEmptyVar}
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
.export HOST_CC
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

.endif				# bmake
