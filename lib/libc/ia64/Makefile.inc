# $FreeBSD: src/lib/libc/ia64/Makefile.inc,v 1.8.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#
# Machine dependent definitions for the ia64 architecture.
#

# Long double is 80 bits
GDTOASRCS+=strtorx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/ia64/Symbol.map
