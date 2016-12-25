# $FreeBSD$

.if ${MACHINE_ARCH:Mmips*hf} == ""
CFLAGS+=-DSOFTFLOAT
.endif

LDBL_PREC = 53
SYM_MAPS += ${.CURDIR}/mips/Symbol.map
