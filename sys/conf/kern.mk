
#
# Warning flags for compiling the kernel and components of the kernel:
#
CWARNFLAGS?=	-Wall -Wstrict-prototypes \
		-Wmissing-prototypes -Wpointer-arith -Wcast-qual \
		-Wundef -Wno-pointer-sign ${FORMAT_EXTENSIONS} \
		-Wmissing-include-dirs -fdiagnostics-show-option \
		-Wno-unknown-pragmas -Wswitch \
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
.if ${COMPILER_VERSION} >= 130000
NO_WUNUSED_BUT_SET_VARIABLE=	-Wno-unused-but-set-variable
.endif
.if ${COMPILER_VERSION} >= 140000
NO_WBITWISE_INSTEAD_OF_LOGICAL=	-Wno-bitwise-instead-of-logical
.endif
.if ${COMPILER_VERSION} >= 150000
NO_WSTRICT_PROTOTYPES=		-Wno-strict-prototypes
NO_WDEPRECATED_NON_PROTOTYPE=	-Wno-deprecated-non-prototype
.endif
# Several other warnings which might be useful in some cases, but not severe
# enough to error out the whole kernel build.  Display them anyway, so there is
# some incentive to fix them eventually.
CWARNEXTRA?=	-Wno-error=tautological-compare -Wno-error=empty-body \
		-Wno-error=parentheses-equality -Wno-error=unused-function \
		-Wno-error=pointer-sign
CWARNEXTRA+=	-Wno-error=shift-negative-value
CWARNEXTRA+=	-Wno-address-of-packed-member
.endif	# clang

.if ${COMPILER_TYPE} == "gcc"
# Catch-all for all the things that are in our tree, but for which we're
# not yet ready for this compiler.
NO_WUNUSED_BUT_SET_VARIABLE=-Wno-unused-but-set-variable
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
		-Wno-error=unused-function
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
		-Wno-alloc-size-larger-than			\
		-Wno-error=alloca-larger-than=
.if ${COMPILER_VERSION} >= 120100
CWARNEXTRA+=	-Wno-error=nonnull				\
		-Wno-dangling-pointer				\
		-Wno-zero-length-bounds
NO_WINFINITE_RECURSION=	-Wno-infinite-recursion
NO_WSTRINGOP_OVERREAD=	-Wno-stringop-overread
.endif
.endif

# GCC produces false positives for functions that switch on an
# enum (GCC bug 87950)
CWARNFLAGS+=	-Wno-return-type
.endif	# gcc

# This warning is utter nonsense
CWARNFLAGS+=	-Wno-format-zero-length

# External compilers may not support our format extensions.  Allow them
# to be disabled.  WARNING: format checking is disabled in this case.
.if ${MK_FORMAT_EXTENSIONS} == "no"
FORMAT_EXTENSIONS=	-Wno-format
.elif ${COMPILER_TYPE} == "clang" || \
    (${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} >= 120100)
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
# Build with BTI+PAC
CFLAGS += -mbranch-protection=standard
.if ${LINKER_FEATURES:Mbti-report}
LDFLAGS += -Wl,-zbti-report=error
.endif
# TODO: support outline atomics
CFLAGS += -mno-outline-atomics
INLINE_LIMIT?=	8000
.endif

#
# For RISC-V we specify the soft-float ABI (lp64) to avoid the use of floating
# point registers within the kernel. However, we include the F and D extensions
# in -march so we can have limited floating point support in context switching
# code. This is different than userland where we use a hard-float ABI (lp64d).
#
# We also specify the "medium" code model, which generates code suitable for a
# 2GiB addressing range located at any offset, allowing modules to be located
# anywhere in the 64-bit address space.  Note that clang and GCC refer to this
# code model as "medium" and "medany" respectively.
#
.if ${MACHINE_CPUARCH} == "riscv"
CFLAGS+=	-march=rv64imafdch
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
.if ${MACHINE_ARCH:Mpowerpc64*} != "" && \
    ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} < 160000
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
# Stack Smashing Protection (SSP) support
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
# Kernel Address SANitizer support
#
.if !empty(KASAN_ENABLED)
SAN_CFLAGS+=	-DSAN_NEEDS_INTERCEPTORS -DSAN_INTERCEPTOR_PREFIX=kasan \
		-fsanitize=kernel-address
.if ${COMPILER_TYPE} == "clang"
SAN_CFLAGS+=	-mllvm -asan-stack=true \
		-mllvm -asan-instrument-dynamic-allocas=true \
		-mllvm -asan-globals=true \
		-mllvm -asan-use-after-scope=true \
		-mllvm -asan-instrumentation-with-call-threshold=0 \
		-mllvm -asan-instrument-byval=false
.endif

.if ${MACHINE_CPUARCH} == "aarch64"
# KASAN/ARM64 TODO: -asan-mapping-offset is calculated from:
#	   (VM_KERNEL_MIN_ADDRESS >> KASAN_SHADOW_SCALE_SHIFT) + $offset = KASAN_MIN_ADDRESS
#
#	This is different than amd64, where we have a different
#	KASAN_MIN_ADDRESS, and this offset value should eventually be
#	upstreamed similar to: https://reviews.llvm.org/D98285
#
.if ${COMPILER_TYPE} == "clang"
SAN_CFLAGS+=	-mllvm -asan-mapping-offset=0xdfff208000000000
.else
SAN_CFLAGS+=	-fasan-shadow-offset=0xdfff208000000000
.endif
.elif ${MACHINE_CPUARCH} == "amd64" && \
      ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 180000
# Work around https://github.com/llvm/llvm-project/issues/87923, which leads to
# an assertion failure compiling dtrace.c with asan enabled.
SAN_CFLAGS+=	-mllvm -asan-use-stack-safety=0
.endif
.endif # !empty(KASAN_ENABLED)

#
# Kernel Concurrency SANitizer support
#
.if !empty(KCSAN_ENABLED)
SAN_CFLAGS+=	-DSAN_NEEDS_INTERCEPTORS -DSAN_INTERCEPTOR_PREFIX=kcsan \
		-fsanitize=thread
.endif

#
# Kernel Memory SANitizer support
#
.if !empty(KMSAN_ENABLED)
# Disable -fno-sanitize-memory-param-retval until interceptors have been
# updated to work properly with it.
MSAN_CFLAGS+=	-DSAN_NEEDS_INTERCEPTORS -DSAN_INTERCEPTOR_PREFIX=kmsan \
		-fsanitize=kernel-memory
.if ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 160000
MSAN_CFLAGS+=	-fno-sanitize-memory-param-retval
.endif
SAN_CFLAGS+=	${MSAN_CFLAGS}
.endif # !empty(KMSAN_ENABLED)

#
# Kernel Undefined Behavior SANitizer support
#
.if !empty(KUBSAN_ENABLED)
SAN_CFLAGS+=	-fsanitize=undefined
.endif

#
# Generic Kernel Coverage support
#
.if !empty(COVERAGE_ENABLED)
.if ${COMPILER_TYPE} == "clang" || \
    (${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} >= 80100)
SAN_CFLAGS+=	-fsanitize-coverage=trace-pc,trace-cmp
.else
SAN_CFLAGS+=	-fsanitize-coverage=trace-pc
.endif
.endif # !empty(COVERAGE_ENABLED)

# Add the sanitizer C flags
CFLAGS+=	${SAN_CFLAGS}

#
# Initialize stack variables on function entry
#
.if ${OPT_INIT_ALL} != "none"
.if ${COMPILER_FEATURES:Minit-all}
CFLAGS+= -ftrivial-auto-var-init=${OPT_INIT_ALL}
.if ${OPT_INIT_ALL} == "zero" && ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} < 160000
CFLAGS+= -enable-trivial-auto-var-init-zero-knowing-it-will-be-removed-from-clang
.endif
.else
.warning INIT_ALL (${OPT_INIT_ALL}) requested but not supported by compiler
.endif
.endif

#
# Some newer toolchains default to DWARF 5, which isn't supported by some build
# tools yet.
#
.if (${CFLAGS:M-g} != "" || ${CFLAGS:M-g[0-3]} != "") && ${CFLAGS:M-gdwarf*} == ""
CFLAGS+=	-gdwarf-4
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

CSTD?=		gnu17

# c99/gnu99 is the minimum C standard version supported for kernel build
.if ${CSTD} == "k&r" || ${CSTD} == "c89" || ${CSTD} == "c90" || \
    ${CSTD} == "c94" || ${CSTD} == "c95"
.error "Only c99/gnu99 or later is supported"
.else # CSTD
CFLAGS+=        -std=${CSTD}
.endif # CSTD

NOSAN_CFLAGS= ${CFLAGS:N-fsanitize*:N-fno-sanitize*:N-fasan-shadow-offset*}

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
LD_EMULATION_armv7=armelf_fbsd
LD_EMULATION_i386=elf_i386_fbsd
LD_EMULATION_powerpc= elf32ppc_fbsd
LD_EMULATION_powerpcspe= elf32ppc_fbsd
LD_EMULATION_powerpc64= elf64ppc_fbsd
LD_EMULATION_powerpc64le= elf64lppc_fbsd
LD_EMULATION_riscv64= elf64lriscv
LD_EMULATION=${LD_EMULATION_${MACHINE_ARCH}}
