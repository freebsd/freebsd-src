# $FreeBSD: src/gnu/usr.bin/cc/include/Makefile,v 1.3 2002/09/12 15:56:21 obrien Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}/config/${GCC_CPU}

.if ${TARGET_ARCH} == "i386" || ${TARGET_ARCH} == "x86_64"
INCS=	mmintrin.h xmmintrin.h
.elif ${TARGET_ARCH} == "ia64"
INCS=	ia64intrin.h
.endif

.include <bsd.init.mk>
.include <bsd.incs.mk>
.include <bsd.obj.mk>
