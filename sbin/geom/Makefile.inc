# $FreeBSD$

# Depends on dlopen(3).
WANT_CHERI=	none

GEOM_CLASS_DIR?=/lib/geom

.include "../Makefile.inc"
