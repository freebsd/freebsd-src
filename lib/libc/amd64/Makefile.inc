# $FreeBSD: src/lib/libc/amd64/Makefile.inc,v 1.6.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $
#
# Machine dependent definitions for the amd64 architecture.
#

# Long double is 80 bits
GDTOASRCS+=strtorx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${.CURDIR}/amd64/Symbol.map
