# $FreeBSD$

.include <bsd.own.mk>

LLVM_SRCS= ${.CURDIR}/../../../contrib/llvm

.include "../clang.build.mk"

.PATH: ${LLVM_SRCS}/tools/clang/lib/Headers

INCSDIR=${LIBDIR}/clang/3.6.1/include

INCS=	__stddef_max_align_t.h \
	__wmmintrin_aes.h \
	__wmmintrin_pclmul.h \
	adxintrin.h \
	altivec.h \
	ammintrin.h \
	arm_acle.h \
	avx2intrin.h \
	avx512bwintrin.h \
	avx512erintrin.h \
	avx512fintrin.h \
	avx512vlbwintrin.h \
	avx512vlintrin.h \
	avxintrin.h \
	bmi2intrin.h \
	bmiintrin.h \
	cpuid.h \
	emmintrin.h \
	f16cintrin.h \
	fma4intrin.h \
	fmaintrin.h \
	ia32intrin.h \
	immintrin.h \
	lzcntintrin.h \
	mm3dnow.h \
	mm_malloc.h \
	mmintrin.h \
	module.modulemap \
	nmmintrin.h \
	pmmintrin.h \
	popcntintrin.h \
	prfchwintrin.h \
	rdseedintrin.h \
	rtmintrin.h \
	shaintrin.h \
	smmintrin.h \
	tbmintrin.h \
	tmmintrin.h \
	wmmintrin.h \
	x86intrin.h \
	xmmintrin.h \
	xopintrin.h \
	${GENINCS}
GENINCS= arm_neon.h
CLEANFILES= ${GENINCS} ${GENINCS:C/\.h$/.d/}

# avoid a circular dependency
GENDIRDEPS_FILTER+= Nusr.bin/clang/clang-tblgen.host

.include <bsd.prog.mk>
