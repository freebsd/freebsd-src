# $FreeBSD$

# Long double is 80 bits
GDTOASRCS+=strtorx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${LIBC_SRCTOP}/i386/Symbol.map
