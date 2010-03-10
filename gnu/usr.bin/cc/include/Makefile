# $FreeBSD$

.include "../Makefile.inc"
.include <bsd.own.mk>

.PATH: ${GCCDIR}/config/${GCC_CPU}

.if ${MK_CLANG_IS_CC} == "no"
.if ${TARGET_ARCH} == "i386" || ${TARGET_ARCH} == "amd64"
INCS=	emmintrin.h mmintrin.h pmmintrin.h xmmintrin.h mm_malloc.h
.elif ${TARGET_ARCH} == "ia64"
INCS=	ia64intrin.h
.elif ${TARGET_ARCH} == "arm"
INCS=	mmintrin.h
.elif ${TARGET_ARCH} == "powerpc"
INCS=	ppc-asm.h altivec.h spe.h
.endif
.endif

mm_malloc.h: pmm_malloc.h
	@rm -rf ${.TARGET}
	@cp ${.ALLSRC} ${.TARGET}
CLEANFILES+=	mm_malloc.h

.include <bsd.init.mk>
.include <bsd.incs.mk>
.include <bsd.obj.mk>
