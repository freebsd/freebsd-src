# $FreeBSD$

# Part of a unified Makefile for building kernels.  This part contains all
# of the definitions that need to be before %BEFORE_DEPEND.

# Allow user to configure things that only effect src tree builds.
# Note: This is duplicated from src.sys.mk to ensure that we include
# /etc/src.conf when building the kernel. Kernels can be built without
# the rest of /usr/src, but they still always process SRCCONF even though
# the normal mechanisms to prevent that (compiling out of tree) won't
# work. To ensure they do work, we have to duplicate thee few lines here.
SRCCONF?=	/etc/src.conf
.if (exists(${SRCCONF}) || ${SRCCONF} != "/etc/src.conf") && !target(_srcconf_included_)
.include "${SRCCONF}"
_srcconf_included_:
.endif

.include <bsd.own.mk>
.include <bsd.compiler.mk>
.include "kern.opts.mk"

# Can be overridden by makeoptions or /etc/make.conf
KERNEL_KO?=	kernel
KERNEL?=	kernel
KODIR?=		/boot/${KERNEL}
LDSCRIPT_NAME?=	ldscript.$M
LDSCRIPT?=	$S/conf/${LDSCRIPT_NAME}

M=		${MACHINE_CPUARCH}

AWK?=		awk
CP?=		cp
LINT?=		lint
NM?=		nm
OBJCOPY?=	objcopy
SIZE?=		size

.if defined(DEBUG)
_MINUS_O=	-O
CTFFLAGS+=	-g
.else
.if ${MACHINE_CPUARCH} == "powerpc"
_MINUS_O=	-O	# gcc miscompiles some code at -O2
.else
_MINUS_O=	-O2
.endif
.endif
.if ${MACHINE_CPUARCH} == "amd64"
.if ${COMPILER_TYPE} == "clang"
COPTFLAGS?=-O2 -pipe
.else
COPTFLAGS?=-O2 -frename-registers -pipe
.endif
.else
COPTFLAGS?=${_MINUS_O} -pipe
.endif
.if !empty(COPTFLAGS:M-O[23s]) && empty(COPTFLAGS:M-fno-strict-aliasing)
COPTFLAGS+= -fno-strict-aliasing
.endif
.if !defined(NO_CPU_COPTFLAGS)
COPTFLAGS+= ${_CPUCFLAGS}
.endif
C_DIALECT= -std=c99
NOSTDINC= -nostdinc

INCLUDES= ${NOSTDINC} ${INCLMAGIC} -I. -I$S

# This hack lets us use the OpenBSD altq code without spamming a new
# include path into contrib'ed source files.
INCLUDES+= -I$S/contrib/altq

.if make(depend) || make(kernel-depend)

# ... and the same for ipfilter
INCLUDES+= -I$S/contrib/ipfilter

# ... and the same for ath
INCLUDES+= -I$S/dev/ath -I$S/dev/ath/ath_hal -I$S/contrib/dev/ath/ath_hal

# ... and the same for the NgATM stuff
INCLUDES+= -I$S/contrib/ngatm

# ... and the same for twa
INCLUDES+= -I$S/dev/twa

# ... and the same for cxgb and cxgbe
INCLUDES+= -I$S/dev/cxgb -I$S/dev/cxgbe

.endif

CFLAGS=	${COPTFLAGS} ${C_DIALECT} ${DEBUG} ${CWARNFLAGS}
CFLAGS+= ${INCLUDES} -D_KERNEL -DHAVE_KERNEL_OPTION_HEADERS -include opt_global.h
CFLAGS_PARAM_INLINE_UNIT_GROWTH?=100
CFLAGS_PARAM_LARGE_FUNCTION_GROWTH?=1000
.if ${MACHINE_CPUARCH} == "mips"
CFLAGS_ARCH_PARAMS?=--param max-inline-insns-single=1000
.endif
CFLAGS.gcc+= -fno-common -finline-limit=${INLINE_LIMIT}
CFLAGS.gcc+= --param inline-unit-growth=${CFLAGS_PARAM_INLINE_UNIT_GROWTH}
CFLAGS.gcc+= --param large-function-growth=${CFLAGS_PARAM_LARGE_FUNCTION_GROWTH}
.if defined(CFLAGS_ARCH_PARAMS)
CFLAGS.gcc+=${CFLAGS_ARCH_PARAMS}
.endif
WERROR?= -Werror

# XXX LOCORE means "don't declare C stuff" not "for locore.s".
ASM_CFLAGS= -x assembler-with-cpp -DLOCORE ${CFLAGS}

.if ${COMPILER_TYPE} == "clang"
CLANG_NO_IAS= -no-integrated-as
.else
GCC_MS_EXTENSIONS= -fms-extensions
.endif

.if defined(PROFLEVEL) && ${PROFLEVEL} >= 1
CFLAGS+=	-DGPROF
CFLAGS.gcc+=	-falign-functions=16
.if ${PROFLEVEL} >= 2
CFLAGS+=	-DGPROF4 -DGUPROF
PROF=		-pg
.if ${COMPILER_TYPE} == "gcc"
PROF+=		-mprofiler-epilogue
.endif
.else
PROF=		-pg
.endif
.endif
DEFINED_PROF=	${PROF}

# Put configuration-specific C flags last (except for ${PROF}) so that they
# can override the others.
CFLAGS+=	${CONF_CFLAGS}

# Optional linting. This can be overridden in /etc/make.conf.
LINTFLAGS=	${LINTOBJKERNFLAGS}

NORMAL_C= ${CC} -c ${CFLAGS} ${WERROR} ${PROF} ${.IMPSRC}
NORMAL_S= ${CC} -c ${ASM_CFLAGS} ${WERROR} ${.IMPSRC}
PROFILE_C= ${CC} -c ${CFLAGS} ${WERROR} ${.IMPSRC}
NORMAL_C_NOWERROR= ${CC} -c ${CFLAGS} ${PROF} ${.IMPSRC}

NORMAL_M= ${AWK} -f $S/tools/makeobjops.awk ${.IMPSRC} -c ; \
	  ${CC} -c ${CFLAGS} ${WERROR} ${PROF} ${.PREFIX}.c

NORMAL_FW= uudecode -o ${.TARGET} ${.ALLSRC}
NORMAL_FWO= ${LD} -b binary --no-warn-mismatch -d -warn-common -r \
	-o ${.TARGET} ${.ALLSRC:M*.fw}

# Special flags for managing the compat compiles for ZFS
ZFS_CFLAGS=	-DFREEBSD_NAMECACHE -DBUILDING_ZFS -nostdinc -I$S/cddl/compat/opensolaris -I$S/cddl/contrib/opensolaris/uts/common/fs/zfs -I$S/cddl/contrib/opensolaris/uts/common/zmod -I$S/cddl/contrib/opensolaris/uts/common -I$S -I$S/cddl/contrib/opensolaris/common/zfs -I$S/cddl/contrib/opensolaris/common ${CFLAGS} -Wno-unknown-pragmas -Wno-missing-prototypes -Wno-undef -Wno-strict-prototypes -Wno-cast-qual -Wno-parentheses -Wno-redundant-decls -Wno-missing-braces -Wno-uninitialized -Wno-unused -Wno-inline -Wno-switch -Wno-pointer-arith -Wno-unknown-pragmas
ZFS_CFLAGS+=	-include $S/cddl/compat/opensolaris/sys/debug_compat.h
ZFS_ASM_CFLAGS= -x assembler-with-cpp -DLOCORE ${ZFS_CFLAGS}
ZFS_C=		${CC} -c ${ZFS_CFLAGS} ${WERROR} ${PROF} ${.IMPSRC}
ZFS_S=		${CC} -c ${ZFS_ASM_CFLAGS} ${WERROR} ${.IMPSRC}

.if ${MK_CTF} != "no"
NORMAL_CTFCONVERT=	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.elif ${MAKE_VERSION} >= 5201111300
NORMAL_CTFCONVERT=
.else
NORMAL_CTFCONVERT=	@:
.endif

NORMAL_LINT=	${LINT} ${LINTFLAGS} ${CFLAGS:M-[DIU]*} ${.IMPSRC}

# Infiniband C flags.  Correct include paths and omit errors that linux
# does not honor.
OFEDINCLUDES=	-I$S/ofed/include/
OFEDNOERR=	-Wno-cast-qual -Wno-pointer-arith ${GCC_MS_EXTENSIONS}
OFEDCFLAGS=	${CFLAGS:N-I*} ${OFEDINCLUDES} ${CFLAGS:M-I*} ${OFEDNOERR}
OFED_C_NOIMP=	${CC} -c -o ${.TARGET} ${OFEDCFLAGS} ${WERROR} ${PROF}
OFED_C=		${OFED_C_NOIMP} ${.IMPSRC}

GEN_CFILES= $S/$M/$M/genassym.c ${MFILES:T:S/.m$/.c/}
SYSTEM_CFILES= config.c env.c hints.c vnode_if.c
SYSTEM_DEP= Makefile ${SYSTEM_OBJS}
SYSTEM_OBJS= locore.o ${MDOBJS} ${OBJS}
SYSTEM_OBJS+= ${SYSTEM_CFILES:.c=.o}
SYSTEM_OBJS+= hack.So
SYSTEM_LD= @${LD} -Bdynamic -T ${LDSCRIPT} ${_LDFLAGS} --no-warn-mismatch \
	-warn-common -export-dynamic -dynamic-linker /red/herring \
	-o ${.TARGET} -X ${SYSTEM_OBJS} vers.o
SYSTEM_LD_TAIL= @${OBJCOPY} --strip-symbol gcc2_compiled. ${.TARGET} ; \
	${SIZE} ${.TARGET} ; chmod 755 ${.TARGET}
SYSTEM_DEP+= ${LDSCRIPT}

# MKMODULESENV is set here so that port makefiles can augment
# them.

MKMODULESENV+=	MAKEOBJDIRPREFIX=${.OBJDIR}/modules KMODDIR=${KODIR}
MKMODULESENV+=	MACHINE_CPUARCH=${MACHINE_CPUARCH}
MKMODULESENV+=	MACHINE=${MACHINE} MACHINE_ARCH=${MACHINE_ARCH}
.if (${KERN_IDENT} == LINT)
MKMODULESENV+=	ALL_MODULES=LINT
.endif
.if defined(MODULES_OVERRIDE)
MKMODULESENV+=	MODULES_OVERRIDE="${MODULES_OVERRIDE}"
.endif
.if defined(WITHOUT_MODULES)
MKMODULESENV+=	WITHOUT_MODULES="${WITHOUT_MODULES}"
.endif
.if defined(DEBUG)
MKMODULESENV+=	DEBUG_FLAGS="${DEBUG}"
.endif

# Detect kernel config options that force stack frames to be turned on.
DDB_ENABLED!=	grep DDB opt_ddb.h || true ; echo
DTR_ENABLED!=	grep KDTRACE_FRAME opt_kdtrace.h || true ; echo
HWPMC_ENABLED!=	grep HWPMC opt_hwpmc_hooks.h || true ; echo
