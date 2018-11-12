# $FreeBSD$

SSP_CFLAGS=

SED_FIX_NOTE = -i "" -e '/\.note\.tag/s/progbits/note/'

NO_WMISSING_VARIABLE_DECLARATIONS=

.include "../Makefile.inc"

.if ${MK_STAGING} == "yes" && ${.TARGETS:Nall} == ""
FILES?= ${OBJS}
FILESDIR?= ${LIBDIR}
.endif
