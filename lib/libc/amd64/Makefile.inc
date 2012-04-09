# $FreeBSD: src/lib/libc/amd64/Makefile.inc,v 1.6.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $
#
# Machine dependent definitions for the amd64 architecture.
#

# Long double is 80 bits
GDTOASRCS+=strtorx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/amd64/Symbol.map
