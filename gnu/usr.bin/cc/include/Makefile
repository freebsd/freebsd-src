# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}/config/${GCC_CPU}

.if ${TARGET_ARCH} == "i386" || ${TARGET_ARCH} == "amd64"
INCS=	emmintrin.h mmintrin.h pmmintrin.h xmmintrin.h
.elif ${TARGET_ARCH} == "ia64"
INCS=	ia64intrin.h
.elif ${TARGET_ARCH} == "arm"
INCS=	mmintrin.h
.elif ${TARGET_ARCH} == "powerpc"
INCS=	ppc-asm.h altivec.h spe.h
.endif

NOOBJ=

.include <bsd.init.mk>
.include <bsd.incs.mk>
.include <bsd.obj.mk>
