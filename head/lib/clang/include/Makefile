# $FreeBSD$

.include <bsd.own.mk>

LLVM_SRCS= ${.CURDIR}/../../../contrib/llvm

.include "../clang.build.mk"

.PATH: ${LLVM_SRCS}/tools/clang/lib/Headers

INCSDIR=${LIBDIR}/clang/3.8.0/include

INCS=	__clang_cuda_runtime_wrapper.h \
	__stddef_max_align_t.h \
	__wmmintrin_aes.h \
	__wmmintrin_pclmul.h \
	adxintrin.h \
	altivec.h \
	ammintrin.h \
	arm_acle.h \
	avx2intrin.h \
	avx512bwintrin.h \
	avx512cdintrin.h \
	avx512dqintrin.h \
	avx512erintrin.h \
	avx512fintrin.h \
	avx512vlbwintrin.h \
	avx512vldqintrin.h \
	avx512vlintrin.h \
	avxintrin.h \
	bmi2intrin.h \
	bmiintrin.h \
	cpuid.h \
	cuda_builtin_vars.h \
	emmintrin.h \
	f16cintrin.h \
	fma4intrin.h \
	fmaintrin.h \
	fxsrintrin.h \
	htmintrin.h \
	htmxlintrin.h \
	ia32intrin.h \
	immintrin.h \
	lzcntintrin.h \
	mm3dnow.h \
	mm_malloc.h \
	mmintrin.h \
	module.modulemap \
	nmmintrin.h \
	pkuintrin.h \
	pmmintrin.h \
	popcntintrin.h \
	prfchwintrin.h \
	rdseedintrin.h \
	rtmintrin.h \
	s390intrin.h \
	shaintrin.h \
	smmintrin.h \
	tbmintrin.h \
	tmmintrin.h \
	vadefs.h \
	vecintrin.h \
	wmmintrin.h \
	x86intrin.h \
	xmmintrin.h \
	xopintrin.h \
	xsavecintrin.h \
	xsaveintrin.h \
	xsaveoptintrin.h \
	xsavesintrin.h \
	xtestintrin.h \
	${GENINCS}
GENINCS= arm_neon.h
CLEANFILES= ${GENINCS} ${GENINCS:C/\.h$/.d/}

.include <bsd.prog.mk>
