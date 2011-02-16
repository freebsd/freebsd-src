# $FreeBSD: src/lib/libc/sparc64/Makefile.inc,v 1.6.12.1 2010/12/21 17:10:29 kensmith Exp $
#
# Machine dependent definitions for the ultra sparc architecture.
#

.include "fpu/Makefile.inc"

# Long double is quad precision
GDTOASRCS+=strtopQ.c
MDSRCS+=machdep_ldisQ.c
SYM_MAPS+=${.CURDIR}/sparc64/Symbol.map
