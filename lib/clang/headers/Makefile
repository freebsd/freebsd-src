# $FreeBSD$

.include "../clang.pre.mk"

.PATH:		${CLANG_SRCS}/lib/Headers

INCSDIR=	${LIBDIR}/clang/3.9.0/include

GENINCS+=	arm_neon.h

INCS+=		__clang_cuda_cmath.h
INCS+=		__clang_cuda_intrinsics.h
INCS+=		__clang_cuda_math_forward_declares.h
INCS+=		__clang_cuda_runtime_wrapper.h
INCS+=		__stddef_max_align_t.h
INCS+=		__wmmintrin_aes.h
INCS+=		__wmmintrin_pclmul.h
INCS+=		adxintrin.h
INCS+=		altivec.h
INCS+=		ammintrin.h
INCS+=		arm_acle.h
INCS+=		avx2intrin.h
INCS+=		avx512bwintrin.h
INCS+=		avx512cdintrin.h
INCS+=		avx512dqintrin.h
INCS+=		avx512erintrin.h
INCS+=		avx512fintrin.h
INCS+=		avx512ifmaintrin.h
INCS+=		avx512ifmavlintrin.h
INCS+=		avx512pfintrin.h
INCS+=		avx512vbmiintrin.h
INCS+=		avx512vbmivlintrin.h
INCS+=		avx512vlbwintrin.h
INCS+=		avx512vlcdintrin.h
INCS+=		avx512vldqintrin.h
INCS+=		avx512vlintrin.h
INCS+=		avxintrin.h
INCS+=		bmi2intrin.h
INCS+=		bmiintrin.h
INCS+=		clflushoptintrin.h
INCS+=		cpuid.h
INCS+=		cuda_builtin_vars.h
INCS+=		emmintrin.h
INCS+=		f16cintrin.h
INCS+=		fma4intrin.h
INCS+=		fmaintrin.h
INCS+=		fxsrintrin.h
INCS+=		htmintrin.h
INCS+=		htmxlintrin.h
INCS+=		ia32intrin.h
INCS+=		immintrin.h
INCS+=		lzcntintrin.h
INCS+=		mm3dnow.h
INCS+=		mm_malloc.h
INCS+=		mmintrin.h
INCS+=		module.modulemap
INCS+=		mwaitxintrin.h
INCS+=		nmmintrin.h
INCS+=		opencl-c.h
INCS+=		pkuintrin.h
INCS+=		pmmintrin.h
INCS+=		popcntintrin.h
INCS+=		prfchwintrin.h
INCS+=		rdseedintrin.h
INCS+=		rtmintrin.h
INCS+=		s390intrin.h
INCS+=		shaintrin.h
INCS+=		smmintrin.h
INCS+=		tbmintrin.h
INCS+=		tmmintrin.h
INCS+=		vadefs.h
INCS+=		vecintrin.h
INCS+=		wmmintrin.h
INCS+=		x86intrin.h
INCS+=		xmmintrin.h
INCS+=		xopintrin.h
INCS+=		xsavecintrin.h
INCS+=		xsaveintrin.h
INCS+=		xsaveoptintrin.h
INCS+=		xsavesintrin.h
INCS+=		xtestintrin.h
INCS+=		${GENINCS}

arm_neon.h: ${CLANG_SRCS}/include/clang/Basic/arm_neon.td
	${CLANG_TBLGEN} -gen-arm-neon \
	    -d ${.TARGET:C/$/.d/} -o ${.TARGET} \
	    ${CLANG_SRCS}/include/clang/Basic/arm_neon.td

CLEANFILES=	${GENINCS} ${GENINCS:C/$/.d/}

.include <bsd.prog.mk>
