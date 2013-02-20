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

# convert path to absolute
.if ${MAKE_VERSION:U0} > 20100408
M_tA = tA
.else
M_tA = C,.*,('cd' & \&\& 'pwd') 2> /dev/null || echo &,:sh
.endif

# this is handy for forcing a space into something.
AnEmptyVar=

# absoulte path to what we are reading.
_PARSEDIR = ${.PARSEDIR:${M_tA}}

.if !empty(SB)
SB_SRC ?= ${SB}/src
SB_OBJROOT ?= ${SB}/obj
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

OBJTOP ?= ${OBJROOT}${MACHINE}

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
.endif
.if ${MACHINE} == "host"
OBJTOP := ${HOST_OBJTOP}
.endif

# if you want objdirs make them automatic
.if ${MKOBJDIRS:Uno} == "auto"
WITH_AUTO_OBJ= yes
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
STAGE_MACHINE= ${MACHINE}
.endif
STAGE_OBJTOP= ${STAGE_ROOT}/${STAGE_MACHINE}
STAGE_COMMON_OBJTOP= ${STAGE_ROOT}/common
STAGE_HOST_OBJTOP= ${STAGE_ROOT}/${HOST_TARGET}

STAGE_LIBDIR= ${STAGE_OBJTOP}${LIBDIR:U/lib}
# this is not the same as INCLUDEDIR
STAGE_INCSDIR= ${STAGE_OBJTOP}${INCSDIR:U/include}
# the target is usually an absolute path
STAGE_SYMLINKS_DIR= ${STAGE_OBJTOP}

.ifndef WITH_SYSROOT
.if ${MACHINE} != "host"
CFLAGS_LAST+= -nostdinc
.endif
CFLAGS_LAST+= -isystem ${STAGE_OBJTOP}/usr/include -isystem ${STAGE_OBJTOP}/include
CFLAGS_LAST += ${CFLAGS_LAST.${COMPILER_TYPE}}
LDFLAGS_LAST+= -B${STAGE_LIBDIR} -L${STAGE_LIBDIR}
CXXFLAGS_LAST += -isystem ${STAGE_OBJTOP}/usr/include/c++/${GCCVER:U4.2}
# backward doesn't get searched if -nostdinc
CXXFLAGS_LAST += -isystem ${STAGE_OBJTOP}/usr/include/c++/${GCCVER:U4.2}/backward
CFLAGS_LAST.clang += -isystem ${STAGE_OBJTOP}/usr/include/clang/3.2
CXXFLAGS_LAST += ${CFLAGS_LAST.${COMPILER_TYPE}}
.else
# if ld suppored sysroot, this would suffice
CFLAGS_LAST+= --sysroot=${STAGE_OBJTOP} -isystem ${STAGE_OBJTOP}/include
.endif
.endif
STAGED_INCLUDE_DIR= ${STAGE_OBJTOP}/include
.if ${USE_META:Uyes} == "yes"
.include "meta.sys.mk"
.endif

# most dirs can be satisfied with one Makefile.depend
.undef .MAKE.DEPENDFILE
.MAKE.DEPENDFILE_PREFERENCE = \
	${.MAKE.DEPENDFILE_PREFIX} \
	${.MAKE.DEPENDFILE_PREFIX}.${MACHINE}

.include "sys.dependfile.mk"

.if ${MACHINE} == "host"
# need a machine specific file
.MAKE.DEPENDFILE= ${.MAKE.DEPENDFILE_PREFIX}.${MACHINE}
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
