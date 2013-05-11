WITH_INSTALL_AS_USER= yes

.if defined(.PARSEDIR)		# bmake

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
M_whence = ${M_type}:M/*

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
.export SRCTOP
OBJROOT ?= ${SRCTOP:H}/obj/
.endif

# we need HOST_TARGET etc below.
.include <host-target.mk>

# from src/Makefile (for universe)
TARGET_ARCHES_arm?=     arm armeb armv6 armv6eb
TARGET_ARCHES_mips?=    mipsel mips mips64el mips64 mipsn32
TARGET_ARCHES_powerpc?= powerpc powerpc64
TARGET_ARCHES_pc98?=    i386

# the list of machines we support
ALL_MACHINE_LIST?= amd64 arm i386 ia64 mips pc98 powerpc sparc64
.for m in ${ALL_MACHINE_LIST:O:u}
MACHINE_ARCH_LIST.$m?= ${TARGET_ARCHES_${m}:U$m}
MACHINE_ARCH.$m?= ${MACHINE_ARCH_LIST.$m:[1]}
.endfor
.if empty(MACHINE_ARCH)
MACHINE_ARCH:= ${TARGET_ARCH:U${MACHINE_ARCH.${MACHINE}}}
.endif

.if !defined(_TARGETS)
# some things we do only once
_TARGETS := ${.TARGETS}
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

# if you want objdirs make them automatic
# we need .OBJDIR made before we start populating .PATH
.if ${MKOBJDIRS:Uno} == "auto" || defined(WITH_AUTO_OBJ)
WITH_AUTO_OBJ= yes
MKOBJDIRS=auto
.include <auto.obj.mk>
.endif

# the logic in bsd.own.mk forces this dance
.ifndef WITHOUT_META_MODE
WITH_META_MODE= yes

.ifndef WITHOUT_STAGING
WITH_STAGING= yes
.ifndef WITHOUT_STAGING_PROG
WITH_STAGING_PROG= yes
.endif
.endif

PYTHON ?= /usr/local/bin/python

.if ${.MAKE.LEVEL} == 0
.if ${MAKESYSPATH:Uno:M*.../*} != ""
# make sure this is resolved
MAKESYSPATH:= ${MAKESYSPATH:S,:, ,g:C,\.\.\./.*,${_this:H},:ts:}
.export MAKESYSPATH
.endif
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

.if !empty(STAGE_ROOT)
.if ${MACHINE} == "host"
STAGE_MACHINE= ${HOST_TARGET}
.else
STAGE_MACHINE:= ${TARGET_OBJ_SPEC}
.endif
STAGE_OBJTOP:= ${STAGE_ROOT}/${STAGE_MACHINE}
STAGE_COMMON_OBJTOP:= ${STAGE_ROOT}/common
STAGE_HOST_OBJTOP:= ${STAGE_ROOT}/${HOST_TARGET}

STAGE_LIBDIR= ${STAGE_OBJTOP}${LIBDIR:U/lib}
# this is not the same as INCLUDEDIR
STAGE_INCSDIR= ${STAGE_OBJTOP}${INCSDIR:U/include}
# the target is usually an absolute path
STAGE_SYMLINKS_DIR= ${STAGE_OBJTOP}

.ifndef WITH_SYSROOT
.if ${MACHINE} != "host"
CFLAGS_LAST+= -nostdinc
.endif
CFLAGS_LAST+= -isystem ${STAGE_OBJTOP}/usr/include 
CFLAGS_LAST += ${CFLAGS_LAST.${COMPILER_TYPE}}
LDFLAGS_LAST+= -B${STAGE_LIBDIR} -L${STAGE_LIBDIR}
CXXFLAGS_LAST += -isystem ${STAGE_OBJTOP}/usr/include/c++/${GCCVER:U4.2}
# backward doesn't get searched if -nostdinc
CXXFLAGS_LAST += -isystem ${STAGE_OBJTOP}/usr/include/c++/${GCCVER:U4.2}/backward
CFLAGS_LAST.clang += -isystem ${STAGE_OBJTOP}/usr/include/clang/3.2
CXXFLAGS_LAST += ${CFLAGS_LAST.${COMPILER_TYPE}}
.else
# if ld suppored sysroot, this would suffice
CFLAGS_LAST+= --sysroot=${STAGE_OBJTOP}
.endif
.endif
STAGED_INCLUDE_DIR= ${STAGE_OBJTOP}/usr/include
.if ${USE_META:Uyes} == "yes"
.include "meta.sys.mk"
.endif

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

.MAKE.META.BAILIWICK = ${SB} ${OBJROOT} ${STAGE_ROOT}

.endif				# meta mode

# ensure we have a value
.MAKE.MODE ?= normal

# don't rely on MACHINE_ARCH being set or valid

MACHINE_ARCH.host = ${_HOST_ARCH}
MACHINE_ARCH.${MACHINE} ?= ${MACHINE}
MACHINE_ARCH := ${MACHINE_ARCH.${MACHINE}}

CSU_DIR.i386 = csu/i386-elf
CSU_DIR.${MACHINE_ARCH} ?= csu/${MACHINE_ARCH}
CSU_DIR := ${CSU_DIR.${MACHINE_ARCH}}

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
TIME_STAMP_FMT = @ %s [%Y-%m-%d %T]
TIME_STAMP = ${TIME_STAMP_FMT:localtime}
# this will produce the same output but as of when date(1) is run.
TIME_STAMP_DATE = `date '+${TIME_STAMP_FMT}'`
TIME_STAMP_END?= ${TIME_STAMP_DATE}

.endif				# bmake
