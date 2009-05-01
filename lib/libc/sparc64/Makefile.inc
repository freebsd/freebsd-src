# $FreeBSD: src/lib/libc/sparc64/Makefile.inc,v 1.6.8.1 2009/04/15 03:14:26 kensmith Exp $
#
# Machine dependent definitions for the ultra sparc architecture.
#

.include "fpu/Makefile.inc"

# Long double is quad precision
GDTOASRCS+=strtopQ.c
MDSRCS+=machdep_ldisQ.c
SYM_MAPS+=${.CURDIR}/sparc64/Symbol.map
