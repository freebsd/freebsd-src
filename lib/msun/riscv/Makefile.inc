# $FreeBSD$

.if ${MACHINE_ARCH:Mriscv*sf} != ""
CFLAGS+=-DSOFTFLOAT
.endif

LDBL_PREC = 113
SYM_MAPS += ${.CURDIR}/riscv/Symbol.map
