# $FreeBSD: src/gnu/usr.bin/cc/include/Makefile,v 1.5 2003/04/26 20:04:38 obrien Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}/config/${GCC_CPU}

.if ${TARGET_ARCH} == "i386" || ${TARGET_ARCH} == "amd64"
INCS=	mmintrin.h xmmintrin.h
.elif ${TARGET_ARCH} == "ia64"
INCS=	ia64intrin.h
.endif

NOOBJ=

.include <bsd.init.mk>
.include <bsd.incs.mk>
.include <bsd.obj.mk>
