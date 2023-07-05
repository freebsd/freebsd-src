# $FreeBSD$

.include "../clang.pre.mk"

.PATH:		${CLANG_SRCS}/lib/Headers

INCSGROUPS=	INCS CUDA HLSL OMP ORC PPC
INCSDIR=	${LIBDIR}/clang/16/include
CUDADIR=	${INCSDIR}/cuda_wrappers
HLSLDIR=	${INCSDIR}/hlsl
OMPDIR=		${INCSDIR}/openmp_wrappers
PPCDIR=		${INCSDIR}/ppc_wrappers

INCS+=		__clang_cuda_builtin_vars.h
INCS+=		__clang_cuda_cmath.h
INCS+=		__clang_cuda_complex_builtins.h
INCS+=		__clang_cuda_device_functions.h
INCS+=		__clang_cuda_intrinsics.h
INCS+=		__clang_cuda_libdevice_declares.h
INCS+=		__clang_cuda_math.h
INCS+=		__clang_cuda_math_forward_declares.h
INCS+=		__clang_cuda_runtime_wrapper.h
INCS+=		__clang_cuda_texture_intrinsics.h
INCS+=		__clang_hip_cmath.h
INCS+=		__clang_hip_libdevice_declares.h
INCS+=		__clang_hip_math.h
INCS+=		__clang_hip_runtime_wrapper.h
INCS+=		__clang_hip_stdlib.h
INCS+=		__stddef_max_align_t.h
INCS+=		__wmmintrin_aes.h
INCS+=		__wmmintrin_pclmul.h
INCS+=		adxintrin.h
INCS+=		altivec.h
INCS+=		ammintrin.h
INCS+=		amxfp16intrin.h
INCS+=		amxintrin.h
INCS+=		arm64intr.h
INCS+=		arm_acle.h
INCS+=		arm_cmse.h
INCS+=		arm_neon_sve_bridge.h
INCS+=		armintr.h
INCS+=		avx2intrin.h
INCS+=		avx512bf16intrin.h
INCS+=		avx512bitalgintrin.h
INCS+=		avx512bwintrin.h
INCS+=		avx512cdintrin.h
INCS+=		avx512dqintrin.h
INCS+=		avx512erintrin.h
INCS+=		avx512fintrin.h
INCS+=		avx512fp16intrin.h
INCS+=		avx512ifmaintrin.h
INCS+=		avx512ifmavlintrin.h
INCS+=		avx512pfintrin.h
INCS+=		avx512vbmi2intrin.h
INCS+=		avx512vbmiintrin.h
INCS+=		avx512vbmivlintrin.h
INCS+=		avx512vlbf16intrin.h
INCS+=		avx512vlbitalgintrin.h
INCS+=		avx512vlbwintrin.h
INCS+=		avx512vlcdintrin.h
INCS+=		avx512vldqintrin.h
INCS+=		avx512vlfp16intrin.h
INCS+=		avx512vlintrin.h
INCS+=		avx512vlvbmi2intrin.h
INCS+=		avx512vlvnniintrin.h
INCS+=		avx512vlvp2intersectintrin.h
INCS+=		avx512vnniintrin.h
INCS+=		avx512vp2intersectintrin.h
INCS+=		avx512vpopcntdqintrin.h
INCS+=		avx512vpopcntdqvlintrin.h
INCS+=		avxifmaintrin.h
INCS+=		avxintrin.h
INCS+=		avxneconvertintrin.h
INCS+=		avxvnniint8intrin.h
INCS+=		avxvnniintrin.h
INCS+=		bmi2intrin.h
INCS+=		bmiintrin.h
INCS+=		builtins.h
INCS+=		cet.h
INCS+=		cetintrin.h
INCS+=		cldemoteintrin.h
INCS+=		clflushoptintrin.h
INCS+=		clwbintrin.h
INCS+=		clzerointrin.h
INCS+=		cmpccxaddintrin.h
INCS+=		cpuid.h
INCS+=		crc32intrin.h
INCS+=		emmintrin.h
INCS+=		enqcmdintrin.h
INCS+=		f16cintrin.h
INCS+=		fma4intrin.h
INCS+=		fmaintrin.h
INCS+=		fxsrintrin.h
INCS+=		gfniintrin.h
INCS+=		hexagon_circ_brev_intrinsics.h
INCS+=		hexagon_protos.h
INCS+=		hexagon_types.h
INCS+=		hlsl.h
INCS+=		hresetintrin.h
INCS+=		htmintrin.h
INCS+=		htmxlintrin.h
INCS+=		hvx_hexagon_protos.h
INCS+=		ia32intrin.h
INCS+=		immintrin.h
INCS+=		invpcidintrin.h
INCS+=		keylockerintrin.h
INCS+=		larchintrin.h
INCS+=		lwpintrin.h
INCS+=		lzcntintrin.h
INCS+=		mm3dnow.h
INCS+=		mm_malloc.h
INCS+=		mmintrin.h
INCS+=		module.modulemap
INCS+=		movdirintrin.h
INCS+=		msa.h
INCS+=		mwaitxintrin.h
INCS+=		nmmintrin.h
INCS+=		opencl-c-base.h
INCS+=		opencl-c.h
INCS+=		pconfigintrin.h
INCS+=		pkuintrin.h
INCS+=		pmmintrin.h
INCS+=		popcntintrin.h
INCS+=		prfchiintrin.h
INCS+=		prfchwintrin.h
INCS+=		ptwriteintrin.h
INCS+=		raointintrin.h
INCS+=		rdpruintrin.h
INCS+=		rdseedintrin.h
INCS+=		rtmintrin.h
INCS+=		s390intrin.h
INCS+=		serializeintrin.h
INCS+=		sgxintrin.h
INCS+=		shaintrin.h
INCS+=		smmintrin.h
INCS+=		tbmintrin.h
INCS+=		tmmintrin.h
INCS+=		tsxldtrkintrin.h
INCS+=		uintrintrin.h
INCS+=		vadefs.h
INCS+=		vaesintrin.h
INCS+=		vecintrin.h
INCS+=		velintrin.h
INCS+=		velintrin_approx.h
INCS+=		velintrin_gen.h
INCS+=		vpclmulqdqintrin.h
INCS+=		waitpkgintrin.h
INCS+=		wasm_simd128.h
INCS+=		wbnoinvdintrin.h
INCS+=		wmmintrin.h
INCS+=		x86gprintrin.h
INCS+=		x86intrin.h
INCS+=		xmmintrin.h
INCS+=		xopintrin.h
INCS+=		xsavecintrin.h
INCS+=		xsaveintrin.h
INCS+=		xsaveoptintrin.h
INCS+=		xsavesintrin.h
INCS+=		xtestintrin.h
INCS+=		${GENINCS}

# Headers which possibly conflict with our own versions:
.ifdef INSTALL_CONFLICTING_CLANG_HEADERS
INCS+=		float.h
INCS+=		intrin.h
INCS+=		inttypes.h
INCS+=		iso646.h
INCS+=		limits.h
INCS+=		stdalign.h
INCS+=		stdarg.h
INCS+=		stdatomic.h
INCS+=		stdbool.h
INCS+=		stddef.h
INCS+=		stdint.h
INCS+=		stdnoreturn.h
INCS+=		tgmath.h
INCS+=		unwind.h
INCS+=		varargs.h
.endif # INSTALL_CONFLICTING_CLANG_HEADERS

CUDA+=		cuda_wrappers/algorithm
CUDA+=		cuda_wrappers/cmath
CUDA+=		cuda_wrappers/complex
CUDA+=		cuda_wrappers/new

HLSL+=		hlsl/hlsl_basic_types.h
HLSL+=		hlsl/hlsl_intrinsics.h

OMP+=		openmp_wrappers/__clang_openmp_device_functions.h
OMP+=		openmp_wrappers/cmath
OMP+=		openmp_wrappers/complex
OMP+=		openmp_wrappers/complex.h
OMP+=		openmp_wrappers/complex_cmath.h
OMP+=		openmp_wrappers/math.h
OMP+=		openmp_wrappers/new

PPC+=		ppc_wrappers/bmi2intrin.h
PPC+=		ppc_wrappers/bmiintrin.h
PPC+=		ppc_wrappers/emmintrin.h
PPC+=		ppc_wrappers/immintrin.h
PPC+=		ppc_wrappers/mm_malloc.h
PPC+=		ppc_wrappers/mmintrin.h
PPC+=		ppc_wrappers/pmmintrin.h
PPC+=		ppc_wrappers/smmintrin.h
PPC+=		ppc_wrappers/tmmintrin.h
PPC+=		ppc_wrappers/x86gprintrin.h
PPC+=		ppc_wrappers/x86intrin.h
PPC+=		ppc_wrappers/xmmintrin.h

.for hdr in bf16/bf16 cde/cde-header fp16/fp16 mve/mve-header neon/neon \
	sve/sve-header
arm_${hdr:H}.h: ${CLANG_SRCS}/include/clang/Basic/arm_${hdr:H}.td
	${CLANG_TBLGEN} -gen-arm-${hdr:T} \
	    -I ${CLANG_SRCS}/include/clang/Basic -d ${.TARGET:C/$/.d/} \
	    -o ${.TARGET} ${CLANG_SRCS}/include/clang/Basic/arm_${hdr:H}.td
GENINCS+=	arm_${hdr:H}.h
.endfor

.for hdr in vector/vector-header
riscv_${hdr:H}.h: ${CLANG_SRCS}/include/clang/Basic/riscv_${hdr:H}.td
	${CLANG_TBLGEN} -gen-riscv-${hdr:T} \
	    -I ${CLANG_SRCS}/include/clang/Basic -d ${.TARGET:C/$/.d/} \
	    -o ${.TARGET} ${CLANG_SRCS}/include/clang/Basic/riscv_${hdr:H}.td
GENINCS+=	riscv_${hdr:H}.h
.endfor

CLEANFILES=	${GENINCS} ${GENINCS:C/$/.d/}

.include <bsd.prog.mk>
