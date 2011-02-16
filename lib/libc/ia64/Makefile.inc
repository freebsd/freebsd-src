# $FreeBSD: src/lib/libc/ia64/Makefile.inc,v 1.8.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $
#
# Machine dependent definitions for the ia64 architecture.
#

# Long double is 80 bits
GDTOASRCS+=strtorx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/ia64/Symbol.map
