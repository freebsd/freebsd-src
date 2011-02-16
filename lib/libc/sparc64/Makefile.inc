# $FreeBSD: src/lib/libc/sparc64/Makefile.inc,v 1.7.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $
#
# Machine dependent definitions for the ultra sparc architecture.
#

.include "fpu/Makefile.inc"

# Long double is quad precision
GDTOASRCS+=strtorQ.c
MDSRCS+=machdep_ldisQ.c
SYM_MAPS+=${.CURDIR}/sparc64/Symbol.map
