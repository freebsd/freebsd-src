# $FreeBSD$

#
# Warning flags for compiling the kernel and components of the kernel:
#
CWARNFLAGS?=	-Wall -Wnested-externs -Wstrict-prototypes \
		-Wmissing-prototypes -Wpointer-arith -Wcast-qual \
		-Wundef -Wno-pointer-sign ${FORMAT_EXTENSIONS} \
		-Wmissing-include-dirs -fdiagnostics-show-option \
		-Wno-unknown-pragmas \
		${CWARNEXTRA}
#
# The following flags are next up for working on:
#	-Wextra

# Disable a few warnings for clang, since there are several places in the
# kernel where fixing them is more trouble than it is worth, or where there is
# a false positive.
.if ${COMPILER_TYPE} == "clang"
NO_WCONSTANT_CONVERSION=	-Wno-error=constant-conversion
NO_WSHIFT_COUNT_NEGATIVE=	-Wno-shift-count-negative
NO_WSHIFT_COUNT_OVERFLOW=	-Wno-shift-count-overflow
NO_WSELF_ASSIGN=		-Wno-self-assign
NO_WUNNEEDED_INTERNAL_DECL=	-Wno-error=unneeded-internal-declaration
NO_WSOMETIMES_UNINITIALIZED=	-Wno-error=sometimes-uninitialized
NO_WCAST_QUAL=			-Wno-error=cast-qual
NO_WTAUTOLOGICAL_POINTER_COMPARE= -Wno-tautological-pointer-compare
.if ${COMPILER_VERSION} >= 100000
NO_WMISLEADING_INDENTATION=	-Wno-misleading-indentation
.endif
# Several other warnings which might be useful in some cases, but not severe
# enough to error out the whole kernel build.  Display them anyway, so there is
# some incentive to fix them eventually.
CWARNEXTRA?=	-Wno-error=tautological-compare -Wno-error=empty-body \
		-Wno-error=parentheses-equality -Wno-error=unused-function \
		-Wno-error=pointer-sign
CWARNEXTRA+=	-Wno-error=shift-negative-value
CWARNEXTRA+=	-Wno-address-of-packed-member
.if ${COMPILER_VERSION} >= 130000
CWARNFLAGS+=	-Wno-error=unused-but-set-variable
.endif
.endif	# clang

.if ${COMPILER_TYPE} == "gcc"
# Catch-all for all the things that are in our tree, but for which we're
# not yet ready for this compiler.
NO_WUNUSED_BUT_SET_VARIABLE = -Wno-unused-but-set-variable
CWARNEXTRA?=	-Wno-error=address				\
		-Wno-error=aggressive-loop-optimizations	\
		-Wno-error=array-bounds				\
		-Wno-error=attributes				\
		-Wno-error=cast-qual				\
		-Wno-error=enum-compare				\
		-Wno-error=maybe-uninitialized			\
		-Wno-error=misleading-indentation		\
		-Wno-error=nonnull-compare			\
		-Wno-error=overflow				\
		-Wno-error=sequence-point			\
		-Wno-error=shift-overflow			\
		-Wno-error=tautological-compare			\
		-Wno-unused-but-set-variable
.if ${COMPILER_VERSION} >= 70100
CWARNEXTRA+=	-Wno-error=stringop-overflow
.endif
.if ${COMPILER_VERSION} >= 70200
CWARNEXTRA+=	-Wno-error=memset-elt-size
.endif
.if ${COMPILER_VERSION} >= 80000
CWARNEXTRA+=	-Wno-error=packed-not-aligned
.endif
.if ${COMPILER_VERSION} >= 90100
CWARNEXTRA+=	-Wno-address-of-packed-member			\
		-Wno-error=alloca-larger-than=
.endif
.endif	# gcc

# This warning is utter nonsense
CWARNFLAGS+=	-Wno-format-zero-length

# External compilers may not support our format extensions.  Allow them
# to be disabled.  WARNING: format checking is disabled in this case.
.if ${MK_FORMAT_EXTENSIONS} == "no"
FORMAT_EXTENSIONS=	-Wno-format
.elif ${COMPILER_TYPE} == "clang"
FORMAT_EXTENSIONS=	-D__printf__=__freebsd_kprintf__
.else
FORMAT_EXTENSIONS=	-fformat-extensions
.endif

#
# On i386, do not align the stack to 16-byte boundaries.  Otherwise GCC 2.95
# and above adds code to the entry and exit point of every function to align the
# stack to 16-byte boundaries -- thus wasting approximately 12 bytes of stack
# per function call.  While the 16-byte alignment may benefit micro benchmarks,
# it is probably an overall loss as it makes the code bigger (less efficient
# use of code cache tag lines) and uses more stack (less efficient use of data
# cache tag lines).  Explicitly prohibit the use of FPU, SSE and other SIMD
# operations inside the kernel itself.  These operations are exclusively
# reserved for user applications.
#
# gcc:
# Setting -mno-mmx implies -mno-3dnow
# Setting -mno-sse implies -mno-sse2, -mno-sse3 and -mno-ssse3
#
# clang:
# Setting -mno-mmx implies -mno-3dnow and -mno-3dnowa
# Setting -mno-sse implies -mno-sse2, -mno-sse3, -mno-ssse3, -mno-sse41 and -mno-sse42
#
.if ${MACHINE_CPUARCH} == "i386"
CFLAGS.gcc+=	-mpreferred-stack-boundary=2
CFLAGS.clang+=	-mno-aes -mno-avx
CFLAGS+=	-mno-mmx -mno-sse -msoft-float
INLINE_LIMIT?=	8000
.endif

.if ${MACHINE_CPUARCH} == "arm"
INLINE_LIMIT?=	8000
.endif

.if ${MACHINE_CPUARCH} == "aarch64"
# We generally don't want fpu instructions in the kernel.
CFLAGS += -mgeneral-regs-only
# Reserve x18 for pcpu data
CFLAGS += -ffixed-x18
INLINE_LIMIT?=	8000
.endif

#
# For RISC-V we specify the soft-float ABI (lp64) to avoid the use of floating
# point registers within the kernel. However, for kernels supporting hardware
# float (FPE), we have to include that in the march so we can have limited
# floating point support in context switching needed for that. This is different
# than userland where we use a hard-float ABI (lp64d).
#
# We also specify the "medium" code model, which generates code suitable for a
# 2GiB addressing range located at any offset, allowing modules to be located
# anywhere in the 64-bit address space.  Note that clang and GCC refer to this
# code model as "medium" and "medany" respectively.
#
.if ${MACHINE_CPUARCH} == "riscv"
CFLAGS+=	-march=rv64imafdc
CFLAGS+=	-mabi=lp64
CFLAGS.clang+=	-mcmodel=medium
CFLAGS.gcc+=	-mcmodel=medany
INLINE_LIMIT?=	8000

.if ${LINKER_FEATURES:Mriscv-relaxations} == ""
CFLAGS+=	-mno-relax
.endif
.endif

#
# For AMD64, we explicitly prohibit the use of FPU, SSE and other SIMD
# operations inside the kernel itself.  These operations are exclusively
# reserved for user applications.
#
# gcc:
# Setting -mno-mmx implies -mno-3dnow
# Setting -mno-sse implies -mno-sse2, -mno-sse3, -mno-ssse3 and -mfpmath=387
#
# clang:
# Setting -mno-mmx implies -mno-3dnow and -mno-3dnowa
# Setting -mno-sse implies -mno-sse2, -mno-sse3, -mno-ssse3, -mno-sse41 and -mno-sse42
# (-mfpmath= is not supported)
#
.if ${MACHINE_CPUARCH} == "amd64"
CFLAGS.clang+=	-mno-aes -mno-avx
CFLAGS+=	-mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -msoft-float \
		-fno-asynchronous-unwind-tables
INLINE_LIMIT?=	8000
.endif

#
# For PowerPC we tell gcc to use floating point emulation.  This avoids using
# floating point registers for integer operations which it has a tendency to do.
# Also explicitly disable Altivec instructions inside the kernel.
#
.if ${MACHINE_CPUARCH} == "powerpc"
CFLAGS+=	-mno-altivec -msoft-float
INLINE_LIMIT?=	15000
.endif

.if ${MACHINE_ARCH} == "powerpcspe"
CFLAGS.gcc+=	-mno-spe
.endif

#
# Use dot symbols (or, better, the V2 ELF ABI) on powerpc64 to make
# DDB happy. ELFv2, if available, has some other efficiency benefits.
#
.if ${MACHINE_ARCH:Mpowerpc64*} != ""
CFLAGS+=	-mabi=elfv2
.endif

#
# GCC 3.0 and above like to do certain optimizations based on the
# assumption that the program is linked against libc.  Stop this.
#
CFLAGS+=	-ffreestanding

#
# The C standard leaves signed integer overflow behavior undefined.
# gcc and clang opimizers take advantage of this.  The kernel makes
# use of signed integer wraparound mechanics so we need the compiler
# to treat it as a wraparound and not take shortcuts.
#
CFLAGS+=	-fwrapv

#
# GCC SSP support
#
.if ${MK_SSP} != "no"
CFLAGS+=	-fstack-protector
.endif

#
# Retpoline speculative execution vulnerability mitigation (CVE-2017-5715)
#
.if defined(COMPILER_FEATURES) && ${COMPILER_FEATURES:Mretpoline} != "" && \
    ${MK_KERNEL_RETPOLINE} != "no"
CFLAGS+=	-mretpoline
.endif

#
# Initialize stack variables on function entry
#
.if ${MK_INIT_ALL_ZERO} == "yes"
.if ${COMPILER_FEATURES:Minit-all}
CFLAGS+= -ftrivial-auto-var-init=zero \
    -enable-trivial-auto-var-init-zero-knowing-it-will-be-removed-from-clang
.else
.warning InitAll (zeros) requested but not support by compiler
.endif
.elif ${MK_INIT_ALL_PATTERN} == "yes"
.if ${COMPILER_FEATURES:Minit-all}
CFLAGS+= -ftrivial-auto-var-init=pattern
.else
.warning InitAll (pattern) requested but not support by compiler
.endif
.endif

CFLAGS+= ${CWARNFLAGS:M*} ${CWARNFLAGS.${.IMPSRC:T}}
CFLAGS+= ${CWARNFLAGS.${COMPILER_TYPE}}
CFLAGS+= ${CFLAGS.${COMPILER_TYPE}} ${CFLAGS.${.IMPSRC:T}}

# Tell bmake not to mistake standard targets for things to be searched for
# or expect to ever be up-to-date.
PHONY_NOTMAIN = afterdepend afterinstall all beforedepend beforeinstall \
		beforelinking build build-tools buildfiles buildincludes \
		checkdpadd clean cleandepend cleandir cleanobj configure \
		depend distclean distribute exe \
		html includes install installfiles installincludes \
		obj objlink objs objwarn \
		realinstall regress \
		tags whereobj

.PHONY: ${PHONY_NOTMAIN}
.NOTMAIN: ${PHONY_NOTMAIN}

CSTD=		c99

.if ${CSTD} == "k&r"
CFLAGS+=        -traditional
.elif ${CSTD} == "c89" || ${CSTD} == "c90"
CFLAGS+=        -std=iso9899:1990
.elif ${CSTD} == "c94" || ${CSTD} == "c95"
CFLAGS+=        -std=iso9899:199409
.elif ${CSTD} == "c99"
CFLAGS+=        -std=iso9899:1999
.else # CSTD
CFLAGS+=        -std=${CSTD}
.endif # CSTD

# Please keep this if in sync with bsd.sys.mk
.if ${LD} != "ld" && (${CC:[1]:H} != ${LD:[1]:H} || ${LD:[1]:T} != "ld")
# Add -fuse-ld=${LD} if $LD is in a different directory or not called "ld".
.if ${COMPILER_TYPE} == "clang"
# Note: Clang does not like relative paths for ld so we map ld.lld -> lld.
.if ${COMPILER_VERSION} >= 120000
CCLDFLAGS+=	--ld-path=${LD:[1]:S/^ld.//1W}
.else
CCLDFLAGS+=	-fuse-ld=${LD:[1]:S/^ld.//1W}
.endif
.else
# GCC does not support an absolute path for -fuse-ld so we just print this
# warning instead and let the user add the required symlinks.
# However, we can avoid this warning if -B is set appropriately (e.g. for
# CROSS_TOOLCHAIN=...-gcc).
.if !(${LD:[1]:T} == "ld" && ${CC:tw:M-B${LD:[1]:H}/})
.warning LD (${LD}) is not the default linker for ${CC} but -fuse-ld= is not supported
.endif
.endif
.endif

# Set target-specific linker emulation name.
LD_EMULATION_aarch64=aarch64elf
LD_EMULATION_amd64=elf_x86_64_fbsd
LD_EMULATION_arm=armelf_fbsd
LD_EMULATION_armv6=armelf_fbsd
LD_EMULATION_armv7=armelf_fbsd
LD_EMULATION_i386=elf_i386_fbsd
LD_EMULATION_powerpc= elf32ppc_fbsd
LD_EMULATION_powerpcspe= elf32ppc_fbsd
LD_EMULATION_powerpc64= elf64ppc_fbsd
LD_EMULATION_powerpc64le= elf64lppc_fbsd
LD_EMULATION_riscv64= elf64lriscv
LD_EMULATION_riscv64sf= elf64lriscv
LD_EMULATION=${LD_EMULATION_${MACHINE_ARCH}}
