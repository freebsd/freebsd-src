# $FreeBSD: src/lib/libc/amd64/Makefile.inc,v 1.5.8.1 2009/04/15 03:14:26 kensmith Exp $
#
# Machine dependent definitions for the amd64 architecture.
#

# Long double is 80 bits
GDTOASRCS+=strtopx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/amd64/Symbol.map
