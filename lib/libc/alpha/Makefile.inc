# $FreeBSD$
#
# Machine dependent definitions for the alpha architecture.
#

# On Alpha, long double is just double precision.
MDSRCS+=machdep_ldisd.c
SYM_MAPS+=${.CURDIR}/alpha/Symbol.map
