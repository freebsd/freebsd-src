# $Id: sys.dependfile.mk,v 1.4 2012/11/08 18:31:42 sjg Exp $
#
#	@(#) Copyright (c) 2012, Simon J. Gerraty
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

# This only makes sense in meta mode.
# This allows a mixture of auto generated as well as manually edited
# dependency files, which can be differentiated by their names.
# As per dirdeps.mk we only require:
# 1. a common prefix
# 2. that machine specific files end in .${MACHINE}
#
# The .MAKE.DEPENDFILE_PREFERENCE below is an example.

# All depend file names should start with this
.MAKE.DEPENDFILE_PREFIX ?= Makefile.depend

# The order of preference: we will use the first one of these we find
# otherwise the 1st entry will be used by default.
.MAKE.DEPENDFILE_PREFERENCE ?= \
	${.CURDIR}/${.MAKE.DEPENDFILE_PREFIX}.${MACHINE} \
	${.CURDIR}/${.MAKE.DEPENDFILE_PREFIX}

_e := ${.MAKE.DEPENDFILE_PREFERENCE:@m@${exists($m):?$m:}@}
.if !empty(_e)
.MAKE.DEPENDFILE := ${_e:[1]}
.elif ${.MAKE.DEPENDFILE_PREFERENCE:M*${MACHINE}} != "" && ${.MAKE.DEPENDFILE_PREFERENCE:[1]:E} != ${MACHINE}
# MACHINE specific depend files are supported, but *not* default.
# If any already exist, we should follow suit.
_aml = ${ALL_MACHINE_LIST:Uarm amd64 i386 powerpc:N${MACHINE}} ${MACHINE}
# MACHINE must be the last entry in _aml ;-)
_e := ${_aml:@MACHINE@${.MAKE.DEPENDFILE_PREFERENCE:@m@${exists($m):?$m:}@}@}
.if !empty(_e)
.MAKE.DEPENDFILE ?= ${.MAKE.DEPENDFILE_PREFERENCE:M*${MACHINE}:[1]}
.endif
.endif
.MAKE.DEPENDFILE ?= ${.MAKE.DEPENDFILE_PREFERENCE:[1]}
