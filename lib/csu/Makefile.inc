# $FreeBSD$

SSP_CFLAGS=

SED_FIX_NOTE = -i "" -e '/\.note\.tag/s/progbits/note/'

.include "../Makefile.inc"
