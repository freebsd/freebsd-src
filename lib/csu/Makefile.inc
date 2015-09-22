# $FreeBSD$

SSP_CFLAGS=

SED_FIX_NOTE = -i "" -e '/\.note\.tag/s/progbits/note/'

NO_WMISSING_VARIABLE_DECLARATIONS=

.include "../Makefile.inc"
