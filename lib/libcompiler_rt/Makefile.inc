.include <bsd.compiler.mk>

CRTARCH=	${MACHINE_CPUARCH:C/amd64/x86_64/:C/powerpc/ppc/}

CRTSRC=		${SRCTOP}/contrib/llvm-project/compiler-rt/lib/builtins

.PATH:		${CRTSRC}/${CRTARCH}
.PATH:		${CRTSRC}

SRCF+=		absvdi2
SRCF+=		absvsi2
SRCF+=		absvti2
SRCF+=		addvdi3
SRCF+=		addvsi3
SRCF+=		addvti3
SRCF+=		apple_versioning
SRCF+=		ashldi3
SRCF+=		ashlti3
SRCF+=		ashrdi3
SRCF+=		ashrti3
SRCF+=		bswapdi2
SRCF+=		bswapsi2
SRCF+=		clear_cache
SRCF+=		clzdi2
SRCF+=		clzsi2
SRCF+=		clzti2
SRCF+=		cmpdi2
SRCF+=		cmpti2
SRCF+=		ctzdi2
SRCF+=		ctzsi2
SRCF+=		ctzti2
SRCF+=		divdc3
SRCF+=		divdi3
SRCF+=		divmoddi4
SRCF+=		divmodsi4
SRCF+=		divmodti4
SRCF+=		divsc3
SRCF+=		divsi3
SRCF+=		divti3
SRCF+=		enable_execute_stack
SRCF+=		extendhfsf2
SRCF+=		ffsdi2
SRCF+=		ffssi2
SRCF+=		ffsti2
SRCF+=		fixdfdi
SRCF+=		fixdfti
SRCF+=		fixsfdi
SRCF+=		fixsfti
SRCF+=		fixunsdfdi
SRCF+=		fixunsdfsi
SRCF+=		fixunsdfti
SRCF+=		fixunssfdi
SRCF+=		fixunssfsi
SRCF+=		fixunssfti
SRCF+=		floattidf
SRCF+=		floattisf
SRCF+=		floatunsidf
SRCF+=		floatunsisf
SRCF+=		floatuntidf
SRCF+=		floatuntisf
SRCF+=		int_util
SRCF+=		lshrdi3
SRCF+=		lshrti3
SRCF+=		moddi3
SRCF+=		modsi3
SRCF+=		modti3
SRCF+=		muldc3
SRCF+=		muldi3
SRCF+=		mulodi4
SRCF+=		mulosi4
SRCF+=		muloti4
SRCF+=		mulsc3
SRCF+=		multi3
SRCF+=		mulvdi3
SRCF+=		mulvsi3
SRCF+=		mulvti3
SRCF+=		negdf2
SRCF+=		negdi2
SRCF+=		negsf2
SRCF+=		negti2
SRCF+=		negvdi2
SRCF+=		negvsi2
SRCF+=		negvti2
SRCF+=		paritydi2
SRCF+=		paritysi2
SRCF+=		parityti2
SRCF+=		popcountdi2
SRCF+=		popcountsi2
SRCF+=		popcountti2
SRCF+=		powidf2
SRCF+=		powisf2
SRCF+=		subvdi3
SRCF+=		subvsi3
SRCF+=		subvti3
SRCF+=		trampoline_setup
SRCF+=		truncdfhf2
SRCF+=		truncsfhf2
SRCF+=		ucmpdi2
SRCF+=		ucmpti2
SRCF+=		udivdi3
SRCF+=		udivmoddi4
SRCF+=		udivmodsi4
SRCF+=		udivmodti4
SRCF+=		udivsi3
SRCF+=		udivti3
SRCF+=		umoddi3
SRCF+=		umodsi3
SRCF+=		umodti3

# Enable compiler-rt's atomic implementation only for clang, as it uses clang
# specific builtins, and gcc packages usually come with their own libatomic.
# Exclude arm which has its own implementations of atomic functions, below.
.if "${COMPILER_TYPE}" == "clang" && ${MACHINE_CPUARCH} != "arm"
SRCF+=		atomic
.endif

# Avoid using SSE2 instructions on i386, if unsupported.
.if ${MACHINE_CPUARCH} == "i386" && empty(MACHINE_CPU:Msse2)
SRCS+=		floatdidf.c
SRCS+=		floatdisf.c
SRCS+=		floatundidf.c
SRCS+=		floatundisf.c
.else
SRCF+=		floatdidf
SRCF+=		floatdisf
SRCF+=		floatundidf
SRCF+=		floatundisf
.endif

#
# 80-bit long double functions, only used on x86.
#
.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
SRCF+=		divxc3
SRCF+=		fixxfdi
SRCF+=		fixxfti
SRCF+=		fixunsxfdi
SRCF+=		fixunsxfsi
SRCF+=		fixunsxfti
SRCF+=		floattixf
SRCF+=		floatuntixf
SRCF+=		mulxc3
SRCF+=		powixf2

# Avoid using SSE2 instructions on i386, if unsupported.
.if ${MACHINE_CPUARCH} == "i386" && empty(MACHINE_CPU:Msse2)
SRCS+=		floatdixf.c
SRCS+=		floatundixf.c
.else
SRCF+=		floatdixf
SRCF+=		floatundixf
.endif
.endif

# 128-bit float is an amd64 feature
.if ${MACHINE_CPUARCH} == "amd64"
SRCF+=		extendxftf2
SRCF+=		trunctfxf2
.endif

# __cpu_model support, only used on aarch64 and x86
.if ${MACHINE_CPUARCH} == "aarch64"
SRCS+=		cpu_model/aarch64.c
.elif ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
SRCS+=		cpu_model/x86.c
.endif

# The fp_mode implementation for amd64 and i386 is shared, while other
# architectures use the regular approach.
.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
SRCS+=		i386/fp_mode.c
.else
SRCF+=		fp_mode
.endif

#
# 128-bit quad precision long double support,
# only used on some architectures.
#
.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "riscv"
SRCF+=		addtf3
SRCF+=		comparetf2
SRCF+=		divtc3
SRCF+=		divtf3
SRCF+=		extenddftf2
SRCF+=		extendhftf2
SRCF+=		extendsftf2
SRCF+=		fixtfdi
SRCF+=		fixtfsi
SRCF+=		fixtfti
SRCF+=		fixunstfdi
SRCF+=		fixunstfsi
SRCF+=		fixunstfti
SRCF+=		floatditf
SRCF+=		floatsitf
SRCF+=		floattitf
SRCF+=		floatunditf
SRCF+=		floatunsitf
SRCF+=		floatuntitf
SRCF+=		multc3
SRCF+=		multf3
SRCF+=		powitf2
SRCF+=		subtf3
SRCF+=		trunctfdf2
SRCF+=		trunctfhf2
SRCF+=		trunctfsf2
.endif

# These are already shipped by libc.a on some architectures.
.if ${MACHINE_CPUARCH} != "arm" && ${MACHINE_CPUARCH} != "riscv"
SRCF+=		adddf3
SRCF+=		addsf3
SRCF+=		divdf3
SRCF+=		divsf3
SRCF+=		extendsfdf2
SRCF+=		fixdfsi
SRCF+=		fixsfsi
SRCF+=		floatsidf
SRCF+=		floatsisf
SRCF+=		muldf3
SRCF+=		mulsf3
SRCF+=		subdf3
SRCF+=		subsf3
SRCF+=		truncdfsf2
.endif

.if ${MACHINE_CPUARCH} != "arm"
SRCF+=		comparedf2
SRCF+=		comparesf2
.endif

# Helper to reduce complexity of _Float16 and __bf16 statements below.
.if ${MACHINE_CPUARCH} == "aarch64" || \
    ${MACHINE_CPUARCH} == "amd64" || \
    (${MACHINE_CPUARCH} == "i386" && !empty(MACHINE_CPU:Msse2))
CRT_COMMON_F16_ARCH=t
.endif

#
# _Float16 support, only on some architectures, and with certain compiler
# versions.
#
.if ((${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 150000) && \
     (defined(CRT_COMMON_F16_ARCH) || \
      ${MACHINE_CPUARCH} == "arm" || ${MACHINE_CPUARCH} == "riscv")) || \
    ((${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} >= 120000) && \
     (defined(CRT_COMMON_F16_ARCH)))
CFLAGS+=	-DCOMPILER_RT_HAS_FLOAT16
.endif

#
# __bf16 support, only on some architectures, and with certain compiler
# versions.
#
.if ((${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 150000) && \
     (defined(CRT_COMMON_F16_ARCH))) || \
    ((${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 180000) && \
     ${MACHINE_CPUARCH} == "riscv") || \
    ((${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} >= 130000) && \
     (defined(CRT_COMMON_F16_ARCH)))
CFLAGS+=	-DCOMPILER_RT_HAS_BFLOAT16
SRCF+=		extendbfsf2
SRCF+=		truncdfbf2
SRCF+=		truncsfbf2
.endif

# FreeBSD-specific atomic intrinsics.
.if ${MACHINE_CPUARCH} == "arm"
.PATH:		${SRCTOP}/sys/arm/arm

SRCF+=		stdatomic
CFLAGS+=	-DEMIT_SYNC_ATOMICS
.endif

.for file in ${SRCF}
.if ${MACHINE_CPUARCH} == "arm" && (!defined(CPUTYPE) || ${CPUTYPE:M*soft*} == "") \
    && exists(${CRTSRC}/${CRTARCH}/${file}vfp.S)
SRCS+=		${file}vfp.S
. elif exists(${CRTSRC}/${CRTARCH}/${file}.S)
SRCS+=		${file}.S
. else
SRCS+=		${file}.c
. endif
.endfor

.if ${MACHINE_CPUARCH} == "arm"
SRCS+=		aeabi_div0.c
SRCS+=		aeabi_idivmod.S
SRCS+=		aeabi_ldivmod.S
SRCS+=		aeabi_memcmp.S
SRCS+=		aeabi_memcpy.S
SRCS+=		aeabi_memmove.S
SRCS+=		aeabi_memset.S
SRCS+=		aeabi_uidivmod.S
SRCS+=		aeabi_uldivmod.S
SRCS+=		switch16.S
SRCS+=		switch32.S
SRCS+=		switch8.S
SRCS+=		switchu8.S
SRCS+=		sync_synchronize.S
.endif
