# $FreeBSD: src/lib/libc/sparc64/Makefile.inc,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $
#
# Machine dependent definitions for the ultra sparc architecture.
#

.include "fpu/Makefile.inc"

# Long double is quad precision
GDTOASRCS+=strtopQ.c
MDSRCS+=machdep_ldisQ.c
SYM_MAPS+=${.CURDIR}/sparc64/Symbol.map
