# $FreeBSD$

LDBL_PREC = 53
SYM_MAPS += ${.CURDIR}/arm/Symbol.map

.if ${TARGET_ARCH} == "armv6"
ARCH_SRCS = fenv-softfp.c fenv-vfp.c
.endif

CFLAGS.fenv-vfp.c=	-mfpu=vfp -mfloat-abi=softfp
CFLAGS+=		${CFLAGS.${.IMPSRC:T}}

