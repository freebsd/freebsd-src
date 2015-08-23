# $FreeBSD$

SRCS+=	trivial-vdso_tc.c

# Long double is 64-bits
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${LIBC_SRCTOP}/powerpc/Symbol.map
