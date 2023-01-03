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

# The kernel build always occurs in the object directory which is .CURDIR.
.if ${.MAKE.MODE:Unormal:Mmeta}
.MAKE.MODE+=	curdirOk=yes
.endif

# The kernel build always expects .OBJDIR=.CURDIR.
.OBJDIR: ${.CURDIR}

.if defined(NO_OBJWALK) || ${MK_AUTO_OBJ} == "yes"
NO_OBJWALK=		t
NO_MODULES_OBJ=	t
.endif
.if !defined(NO_OBJWALK)
_obj=		obj
.endif

# Can be overridden by makeoptions or /etc/make.conf
KERNEL_KO?=	kernel
KERNEL?=	kernel
KODIR?=		/boot/${KERNEL}
LDSCRIPT_NAME?=	ldscript.$M
LDSCRIPT?=	$S/conf/${LDSCRIPT_NAME}

M=		${MACHINE}

AWK?=		awk
CP?=		cp
ELFDUMP?=	elfdump
NM?=		nm
OBJCOPY?=	objcopy
SIZE?=		size

.if defined(DEBUG)
CTFFLAGS+=	-g
.endif
.if ${MACHINE_CPUARCH} == "amd64" && ${COMPILER_TYPE} != "clang"
_COPTFLAGS_EXTRA=-frename-registers
.else
_COPTFLAGS_EXTRA=
.endif
COPTFLAGS?=-O2 -pipe ${_COPTFLAGS_EXTRA}
.if !empty(COPTFLAGS:M-O[23s]) && empty(COPTFLAGS:M-fno-strict-aliasing)
COPTFLAGS+= -fno-strict-aliasing
.endif
.if !defined(NO_CPU_COPTFLAGS)
COPTFLAGS+= ${_CPUCFLAGS}
.endif
NOSTDINC= -nostdinc

INCLUDES= ${NOSTDINC} ${INCLMAGIC} -I. -I$S -I$S/contrib/ck/include

CFLAGS=	${COPTFLAGS} ${DEBUG}
CFLAGS+= ${INCLUDES} -D_KERNEL -DHAVE_KERNEL_OPTION_HEADERS -include opt_global.h
CFLAGS_PARAM_INLINE_UNIT_GROWTH?=100
CFLAGS_PARAM_LARGE_FUNCTION_GROWTH?=1000
.if ${MACHINE_CPUARCH} == "mips"
CFLAGS_ARCH_PARAMS?=--param max-inline-insns-single=1000 -DMACHINE_ARCH='"${MACHINE_ARCH}"'
.endif
CFLAGS.gcc+= -fms-extensions -finline-limit=${INLINE_LIMIT}
CFLAGS.gcc+= --param inline-unit-growth=${CFLAGS_PARAM_INLINE_UNIT_GROWTH}
CFLAGS.gcc+= --param large-function-growth=${CFLAGS_PARAM_LARGE_FUNCTION_GROWTH}
CFLAGS.gcc+= -fms-extensions
.if defined(CFLAGS_ARCH_PARAMS)
CFLAGS.gcc+=${CFLAGS_ARCH_PARAMS}
.endif
WERROR?=	-Werror
# The following should be removed no earlier than LLVM11 being imported into the
# tree, to ensure we don't regress the build.  LLVM11 and GCC10 will switch the
# default over to -fno-common, making this redundant.
CFLAGS+=	-fno-common

# XXX LOCORE means "don't declare C stuff" not "for locore.s".
ASM_CFLAGS= -x assembler-with-cpp -DLOCORE ${CFLAGS} ${ASM_CFLAGS.${.IMPSRC:T}}

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

COMPAT_FREEBSD32_ENABLED!= grep COMPAT_FREEBSD32 opt_global.h || true ; echo

KASAN_ENABLED!=	grep KASAN opt_global.h || true ; echo
.if !empty(KASAN_ENABLED)
SAN_CFLAGS+=	-DSAN_NEEDS_INTERCEPTORS -DSAN_INTERCEPTOR_PREFIX=kasan \
		-fsanitize=kernel-address \
		-mllvm -asan-stack=true \
		-mllvm -asan-instrument-dynamic-allocas=true \
		-mllvm -asan-globals=true \
		-mllvm -asan-use-after-scope=true \
		-mllvm -asan-instrumentation-with-call-threshold=0 \
		-mllvm -asan-instrument-byval=false
.endif

KCSAN_ENABLED!=	grep KCSAN opt_global.h || true ; echo
.if !empty(KCSAN_ENABLED)
SAN_CFLAGS+=	-DSAN_NEEDS_INTERCEPTORS -DSAN_INTERCEPTOR_PREFIX=kcsan \
		-fsanitize=thread
.endif

KMSAN_ENABLED!= grep KMSAN opt_global.h || true ; echo
.if !empty(KMSAN_ENABLED)
SAN_CFLAGS+=	-DSAN_NEEDS_INTERCEPTORS -DSAN_INTERCEPTOR_PREFIX=kmsan \
		-fsanitize=kernel-memory
.endif

KUBSAN_ENABLED!=	grep KUBSAN opt_global.h || true ; echo
.if !empty(KUBSAN_ENABLED)
SAN_CFLAGS+=	-fsanitize=undefined
.endif

COVERAGE_ENABLED!=	grep COVERAGE opt_global.h || true ; echo
.if !empty(COVERAGE_ENABLED)
.if ${COMPILER_TYPE} == "clang" || \
    (${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} >= 80100)
SAN_CFLAGS+=	-fsanitize-coverage=trace-pc,trace-cmp
.else
SAN_CFLAGS+=	-fsanitize-coverage=trace-pc
.endif
.endif

CFLAGS+=	${SAN_CFLAGS}

GCOV_ENABLED!=	grep GCOV opt_global.h || true ; echo
.if !empty(GCOV_ENABLED)
.if ${COMPILER_TYPE} == "gcc"
GCOV_CFLAGS+=	 -fprofile-arcs -ftest-coverage
.endif
.endif

CFLAGS+=	${GCOV_CFLAGS}

# Put configuration-specific C flags last (except for ${PROF}) so that they
# can override the others.
CFLAGS+=	${CONF_CFLAGS}

.if defined(LINKER_FEATURES) && ${LINKER_FEATURES:Mbuild-id}
LDFLAGS+=	--build-id=sha1
.endif

.if (${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "i386" || ${MACHINE} == "powerpc") && \
    defined(LINKER_FEATURES) && ${LINKER_FEATURES:Mifunc} == "" && \
    !make(install)
.error amd64/arm64/i386/ppc* kernel requires linker ifunc support
.endif
.if ${MACHINE_CPUARCH} == "amd64"
LDFLAGS+=	-z max-page-size=2097152
.if ${LINKER_TYPE} != "lld"
LDFLAGS+=	-z common-page-size=4096
.else
.if defined(LINKER_FEATURES) && !${LINKER_FEATURES:Mifunc-noplt}
.warning "Linker ${LD} does not support -z ifunc-noplt -> ifunc calls are unoptimized."
.else
LDFLAGS+=	-z notext -z ifunc-noplt
.endif
.endif
.endif  # ${MACHINE_CPUARCH} == "amd64"

.if ${MACHINE_CPUARCH} == "riscv"
# Hack: Work around undefined weak symbols being out of range when linking with
# LLD (address is a PC-relative calculation, and BFD works around this by
# rewriting the instructions to generate an absolute address of 0); -fPIE
# avoids this since it uses the GOT for all extern symbols, which is overly
# inefficient for us. Drop once undefined weak symbols work with medany.
.if ${LINKER_TYPE} == "lld"
CFLAGS+=	-fPIE
.endif
.endif

NORMAL_C= ${CC} -c ${CFLAGS} ${WERROR} ${PROF} ${.IMPSRC}
NORMAL_S= ${CC:N${CCACHE_BIN}} -c ${ASM_CFLAGS} ${WERROR} ${.IMPSRC}
PROFILE_C= ${CC} -c ${CFLAGS} ${WERROR} ${.IMPSRC}
NORMAL_C_NOWERROR= ${CC} -c ${CFLAGS} ${PROF} ${.IMPSRC}

NORMAL_M= ${AWK} -f $S/tools/makeobjops.awk ${.IMPSRC} -c ; \
	  ${CC} -c ${CFLAGS} ${WERROR} ${PROF} ${.PREFIX}.c

NORMAL_FW= uudecode -o ${.TARGET} ${.ALLSRC}
NORMAL_FWO= ${CC:N${CCACHE_BIN}} -c ${ASM_CFLAGS} ${WERROR} -o ${.TARGET} \
	$S/kern/firmw.S -DFIRMW_FILE="${.ALLSRC:M*.fw}" \
	-DFIRMW_SYMBOL="${.ALLSRC:M*.fw:C/[-.\/]/_/g}"

# for ZSTD in the kernel (include zstd/lib/freebsd before other CFLAGS)
ZSTD_C= ${CC} -c -DZSTD_HEAPMODE=1 -I$S/contrib/zstd/lib/freebsd ${CFLAGS} \
	-I$S/contrib/zstd/lib -I$S/contrib/zstd/lib/common ${WERROR} \
	-Wno-missing-prototypes ${PROF} -U__BMI__ \
	-DZSTD_NO_INTRINSICS \
	${.IMPSRC}
# https://github.com/facebook/zstd/commit/812e8f2a [zstd 1.4.1]
# "Note that [GCC] autovectorization still does not do a good job on the
# optimized version, so it's turned off via attribute and flag.  I found
# that neither attribute nor command-line flag were entirely successful in
# turning off vectorization, which is why there were both."
.if ${COMPILER_TYPE} == "gcc"
ZSTD_DECOMPRESS_BLOCK_FLAGS= -fno-tree-vectorize
.endif

ZINCDIR=$S/contrib/openzfs/include
# Common for dtrace / zfs
CDDL_CFLAGS=	\
	-DFREEBSD_NAMECACHE \
	-D_SYS_VMEM_H_ \
	-D__KERNEL \
	-D__KERNEL__ \
	-nostdinc \
	-include $S/modules/zfs/static_ccompile.h \
	-I${ZINCDIR} \
	-I${ZINCDIR}/os/freebsd \
	-I${ZINCDIR}/os/freebsd/spl \
	-I${ZINCDIR}/os/freebsd/zfs  \
	-I$S/modules/zfs \
	-I$S/contrib/openzfs/module/zstd/include \
	${CFLAGS} \
	-Wno-cast-qual \
	-Wno-duplicate-decl-specifier \
	-Wno-missing-braces \
	-Wno-missing-prototypes \
	-Wno-nested-externs \
	-Wno-parentheses \
	-Wno-pointer-arith \
	-Wno-redundant-decls \
	-Wno-strict-prototypes \
	-Wno-switch \
	-Wno-undef \
	-Wno-uninitialized \
	-Wno-unknown-pragmas \
	-Wno-unused \
	-include ${ZINCDIR}/os/freebsd/spl/sys/ccompile.h \
	-I$S/cddl/contrib/opensolaris/uts/common \
	-I$S -I$S/cddl/compat/opensolaris
CDDL_C=		${CC} -c ${CDDL_CFLAGS} ${WERROR} ${PROF} ${.IMPSRC}

# Special flags for managing the compat compiles for ZFS
ZFS_CFLAGS+=	${CDDL_CFLAGS} -DBUILDING_ZFS -DHAVE_UIO_ZEROCOPY \
	-DWITH_NETDUMP -D__KERNEL__ -D_SYS_CONDVAR_H_ -DSMP \
	-DIN_FREEBSD_BASE

.if ${MACHINE_ARCH} == "amd64"
ZFS_CFLAGS+= -DHAVE_AVX2 -DHAVE_AVX -D__x86_64 -DHAVE_SSE2 -DHAVE_AVX512F \
	-DHAVE_SSSE3 -DHAVE_AVX512BW
.endif

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "powerpc" || \
	${MACHINE_ARCH} == "powerpcspe" || ${MACHINE_ARCH} == "arm"
ZFS_CFLAGS+= -DBITS_PER_LONG=32
.else
ZFS_CFLAGS+= -DBITS_PER_LONG=64
.endif


ZFS_ASM_CFLAGS= -x assembler-with-cpp -DLOCORE ${ZFS_CFLAGS}
ZFS_C=		${CC} -c ${ZFS_CFLAGS} ${WERROR} ${PROF} ${.IMPSRC}
ZFS_RPC_C=	${CC} -c ${ZFS_CFLAGS} -DHAVE_RPC_TYPES ${WERROR} ${PROF} ${.IMPSRC}
ZFS_S=		${CC} -c ${ZFS_ASM_CFLAGS} ${WERROR} ${.IMPSRC}



# Special flags for managing the compat compiles for DTrace
DTRACE_CFLAGS=	-DBUILDING_DTRACE ${CDDL_CFLAGS} -I$S/cddl/dev/dtrace -I$S/cddl/dev/dtrace/${MACHINE_CPUARCH}
.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
DTRACE_CFLAGS+=	-I$S/cddl/contrib/opensolaris/uts/intel -I$S/cddl/dev/dtrace/x86
.endif
DTRACE_CFLAGS+=	-I$S/cddl/contrib/opensolaris/common/util -I$S -DDIS_MEM -DSMP -I$S/cddl/compat/opensolaris
DTRACE_CFLAGS+=	-I$S/cddl/contrib/opensolaris/uts/common
DTRACE_ASM_CFLAGS=	-x assembler-with-cpp -DLOCORE ${DTRACE_CFLAGS}
DTRACE_C=	${CC} -c ${DTRACE_CFLAGS}	${WERROR} ${PROF} ${.IMPSRC}
DTRACE_S=	${CC} -c ${DTRACE_ASM_CFLAGS}	${WERROR} ${.IMPSRC}

# Special flags for managing the compat compiles for DTrace/FBT
FBT_CFLAGS=	-DBUILDING_DTRACE -nostdinc -I$S/cddl/dev/fbt/${MACHINE_CPUARCH} -I$S/cddl/dev/fbt ${CDDL_CFLAGS} -I$S/cddl/compat/opensolaris -I$S/cddl/contrib/opensolaris/uts/common  
.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
FBT_CFLAGS+=	-I$S/cddl/dev/fbt/x86
.endif
FBT_C=		${CC} -c ${FBT_CFLAGS}		${WERROR} ${PROF} ${.IMPSRC}

.if ${MK_CTF} != "no"
NORMAL_CTFCONVERT=	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.elif ${MAKE_VERSION} >= 5201111300
NORMAL_CTFCONVERT=
.else
NORMAL_CTFCONVERT=	@:
.endif

# Linux Kernel Programming Interface C-flags
LINUXKPI_INCLUDES=	-I$S/compat/linuxkpi/common/include \
			-I$S/compat/linuxkpi/dummy/include
LINUXKPI_C=		${NORMAL_C} ${LINUXKPI_INCLUDES}

# Infiniband C flags.  Correct include paths and omit errors that linux
# does not honor.
OFEDINCLUDES=	-I$S/ofed/include -I$S/ofed/include/uapi ${LINUXKPI_INCLUDES}
OFEDNOERR=	-Wno-cast-qual -Wno-pointer-arith -Wno-redundant-decls
OFEDCFLAGS=	${CFLAGS:N-I*} -DCONFIG_INFINIBAND_USER_MEM \
		${OFEDINCLUDES} ${CFLAGS:M-I*} ${OFEDNOERR}
OFED_C_NOIMP=	${CC} -c -o ${.TARGET} ${OFEDCFLAGS} ${WERROR} ${PROF}
OFED_C=		${OFED_C_NOIMP} ${.IMPSRC}

# mlxfw C flags.
MLXFW_C=	${OFED_C_NOIMP} \
		-I${SRCTOP}/sys/contrib/xz-embedded/freebsd \
		-I${SRCTOP}/sys/contrib/xz-embedded/linux/lib/xz \
		${.IMPSRC}

GEN_CFILES= $S/$M/$M/genassym.c ${MFILES:T:S/.m$/.c/}
SYSTEM_CFILES= config.c env.c hints.c vnode_if.c
SYSTEM_DEP= Makefile ${SYSTEM_OBJS}
SYSTEM_OBJS= locore.o ${MDOBJS} ${OBJS}
SYSTEM_OBJS+= ${SYSTEM_CFILES:.c=.o}
SYSTEM_OBJS+= hack.pico

KEYMAP=kbdcontrol -P ${SRCTOP}/share/vt/keymaps -P ${SRCTOP}/share/syscons/keymaps
KEYMAP_FIX=sed -e 's/^static keymap_t.* = /static keymap_t key_map = /' -e 's/^static accentmap_t.* = /static accentmap_t accent_map = /'

MD_ROOT_SIZE_CONFIGURED!=	grep MD_ROOT_SIZE opt_md.h || true ; echo
.if ${MFS_IMAGE:Uno} != "no"
.if empty(MD_ROOT_SIZE_CONFIGURED)
SYSTEM_OBJS+= embedfs_${MFS_IMAGE:T:R}.o
.endif
.endif
SYSTEM_LD_BASECMD= \
	${LD} -m ${LD_EMULATION} -Bdynamic -T ${LDSCRIPT} ${_LDFLAGS} \
	--no-warn-mismatch --warn-common --export-dynamic \
	--dynamic-linker /red/herring -X
SYSTEM_LD= @${SYSTEM_LD_BASECMD} -o ${.TARGET} ${SYSTEM_OBJS} vers.o
SYSTEM_LD_TAIL= @${OBJCOPY} --strip-symbol gcc2_compiled. ${.TARGET} ; \
	${SIZE} ${.TARGET} ; chmod 755 ${.TARGET}
SYSTEM_DEP+= ${LDSCRIPT}

# Calculate path for .m files early, if needed.
.if !defined(NO_MODULES) && !defined(__MPATH) && !make(install) && \
    (empty(.MAKEFLAGS:M-V) || defined(NO_SKIP_MPATH))
__MPATH!=find ${S:tA}/ -name \*_if.m
.endif

# MKMODULESENV is set here so that port makefiles can augment
# them.

MKMODULESENV+=	MAKEOBJDIRPREFIX=${.OBJDIR}/modules KMODDIR=${KODIR}
MKMODULESENV+=	MACHINE_CPUARCH=${MACHINE_CPUARCH}
MKMODULESENV+=	MACHINE=${MACHINE} MACHINE_ARCH=${MACHINE_ARCH}
MKMODULESENV+=	MODULES_EXTRA="${MODULES_EXTRA}" WITHOUT_MODULES="${WITHOUT_MODULES}"
MKMODULESENV+=	ARCH_FLAGS="${ARCH_FLAGS}"
.if (${KERN_IDENT} == LINT)
MKMODULESENV+=	ALL_MODULES=LINT
.endif
.if defined(MODULES_OVERRIDE)
MKMODULESENV+=	MODULES_OVERRIDE="${MODULES_OVERRIDE}"
.endif
.if defined(DEBUG)
MKMODULESENV+=	DEBUG_FLAGS="${DEBUG}"
.endif
.if !defined(NO_MODULES)
MKMODULESENV+=	__MPATH="${__MPATH}"
.endif

# Detect kernel config options that force stack frames to be turned on.
DDB_ENABLED!=	grep DDB opt_ddb.h || true ; echo
DTRACE_ENABLED!=grep KDTRACE_FRAME opt_kdtrace.h || true ; echo
HWPMC_ENABLED!=	grep HWPMC opt_hwpmc_hooks.h || true ; echo
