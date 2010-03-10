# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/llvm/tools/clang/lib/Headers

.include <bsd.own.mk>

.if ${MK_CLANG_IS_CC} != "no"
INCS=	emmintrin.h mm_malloc.h mmintrin.h pmmintrin.h tmmintrin.h xmmintrin.h
.endif

.include <bsd.init.mk>
.include <bsd.incs.mk>
.include <bsd.obj.mk>
