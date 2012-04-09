# $FreeBSD: src/lib/libc/ia64/Makefile.inc,v 1.8.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $
#
# Machine dependent definitions for the ia64 architecture.
#

# Long double is 80 bits
GDTOASRCS+=strtorx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/ia64/Symbol.map
