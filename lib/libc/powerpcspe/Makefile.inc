
CFLAGS+= -I${LIBC_SRCTOP}/powerpc

# Long double is 64-bits
SRCS+=machdep_ldisd.c
SYM_MAPS+=${LIBC_SRCTOP}/powerpc/Symbol.map
