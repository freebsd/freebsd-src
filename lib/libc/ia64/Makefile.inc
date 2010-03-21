# $FreeBSD: src/lib/libc/ia64/Makefile.inc,v 1.7.10.1 2010/02/10 00:26:20 kensmith Exp $
#
# Machine dependent definitions for the ia64 architecture.
#

# Long double is 80 bits
GDTOASRCS+=strtopx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/ia64/Symbol.map
