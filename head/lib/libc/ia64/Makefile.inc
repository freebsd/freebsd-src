# $FreeBSD$
#
# Machine dependent definitions for the ia64 architecture.
#

# Long double is 80 bits
GDTOASRCS+=strtorx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${LIBC_SRCTOP}/ia64/Symbol.map
