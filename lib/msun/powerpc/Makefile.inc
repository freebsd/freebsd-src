.if ${MACHINE_ARCH} == "powerpc64le"
LDBL_PREC = 113
ARCH_SRCS = ld128_compat.c
CFLAGS.ld128_compat.c+= -fno-builtin
SYM_MAPS+= ${.CURDIR}/${ARCH_SUBDIR}/Symbol_f128.map
.else
LDBL_PREC = 53
.endif
