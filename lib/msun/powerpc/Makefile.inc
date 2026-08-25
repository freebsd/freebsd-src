.if ${MACHINE_ARCH} == "powerpc64le"
LDBL_PREC = 113
.else
LDBL_PREC = 53
.endif
