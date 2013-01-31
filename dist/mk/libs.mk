# $Id: libs.mk,v 1.2 2007/04/30 17:39:27 sjg Exp $
#
#	@(#) Copyright (c) 2006, Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that 
#	the above copyright notice and this notice are
#	left intact. 
#      
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

.MAIN: all

.if defined(LIBS)

.ifndef LIB
.for t in ${LIBS:R:T:S,^lib,,}
.if make(lib$t)
LIB?= $t
lib$t: all
.endif
.endfor
.endif

.if defined(LIB)
# just one of many
.for v in DPADD SRCS CFLAGS ${LIB_VARS}
$v += ${${v}_lib${LIB}}
.endfor
# ensure that we don't clobber each other's dependencies
DEPENDFILE?= .depend.${LIB}
# lib.mk will do the rest
.else
all: ${LIBS:S,^lib,,:@t@lib$t.a@} .MAKE
.endif
.endif

# handle being called [bsd.]libs.mk
.include <${.PARSEFILE:S,libs,lib,}>

.ifndef LIB
.for t in ${LIBS:R:T:S,^lib,,}
lib$t.a: ${SRCS} ${DPADD} ${SRCS_lib$t} ${DPADD_lib$t} 
	(cd ${.CURDIR} && ${.MAKE} -f ${MAKEFILE} LIB=$t)

clean: $t.clean
$t.clean:
	(cd ${.CURDIR} && ${.MAKE} -f ${MAKEFILE} LIB=$t ${@:E})
.endfor
.endif
