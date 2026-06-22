.if ${MACHINE_ARCH} == "powerpc64le"
# Long double is IEEE binary128
GDTOASRCS+=strtorQ.c
SRCS+=machdep_ldisQ.c
.else
# Long double is 64-bits
SRCS+=machdep_ldisd.c
.endif
SYM_MAPS+=${LIBC_SRCTOP}/powerpc64/Symbol.map
