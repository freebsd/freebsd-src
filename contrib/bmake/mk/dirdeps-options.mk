# $Id: dirdeps-options.mk,v 1.17 2020/08/07 01:57:38 sjg Exp $
#
#	@(#) Copyright (c) 2018-2020, Simon J. Gerraty
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

##
#
# This makefile is used to deal with optional DIRDEPS.
#
# It is to be included by Makefile.depend.options in a
# directory which has DIRDEPS affected by optional features.
# Makefile.depend.options should set DIRDEPS_OPTIONS and
# may also set specific DIRDEPS.* for those options.
#
# If a Makefile.depend.options file exists, it will be included by
# dirdeps.mk and meta.autodep.mk
#
# We include local.dirdeps-options.mk which may also define DIRDEPS.*
# for options.
#
# Thus a directory, that is affected by an option FOO would have
# a Makefile.depend.options that sets
# DIRDEPS_OPTIONS= FOO
# It can also set either/both of
# DIRDEPS.FOO.yes
# DIRDEPS.FOO.no
# to whatever applies for that dir, or it can rely on globals
# set in local.dirdeps-options.mk
# Either way, we will .undef DIRDEPS.* when done.
#
# In some cases the value of MK_FOO might depend on TARGET_SPEC
# so we qualify MK_FOO with .${TARGET_SPEC} and each component
# TARGET_SPEC_VAR (in reverse order) before using MK_FOO.
#

# This should have been set by Makefile.depend.options
# before including us
DIRDEPS_OPTIONS ?=

# pickup any DIRDEPS.* we need
.-include <local.dirdeps-options.mk>

.if ${.MAKE.LEVEL} == 0
# :U below avoids potential errors when we :=
# some options can depend on TARGET_SPEC!
DIRDEPS_OPTIONS_QUALIFIER_LIST ?= \
	${DEP_TARGET_SPEC:U${TARGET_SPEC}} \
	${TARGET_SPEC_VARSr:U${TARGET_SPEC_VARS}:@v@${DEP_$v:U${$v}}@}
# note that we need to include $o in the variable _o$o
# to ensure correct evaluation.
.for o in ${DIRDEPS_OPTIONS}
.undef _o$o _v$o
.for x in ${DIRDEPS_OPTIONS_QUALIFIER_LIST}
.if defined(MK_$o.$x)
_o$o ?= MK_$o.$x
_v$o ?= ${MK_$o.$x}
.endif
.endfor
_v$o ?= ${MK_$o}
.if ${_debug_reldir:U0}
.info ${DEP_RELDIR:U${RELDIR}}.${DEP_TARGET_SPEC:U${TARGET_SPEC}}: o=$o ${_o$o:UMK_$o}=${_v$o:U} DIRDEPS += ${DIRDEPS.$o.${_v$o:U}:U}
.endif
DIRDEPS += ${DIRDEPS.$o.${_v$o:U}:U}
.endfor
DIRDEPS := ${DIRDEPS:O:u}
.if ${_debug_reldir:U0}
.info ${DEP_RELDIR:U${RELDIR}}: DIRDEPS=${DIRDEPS}
.endif
# avoid cross contamination
.for o in ${DIRDEPS_OPTIONS}
.undef DIRDEPS.$o.yes
.undef DIRDEPS.$o.no
.undef _o$o
.undef _v$o
.endfor
.else
# whether options are enabled or not,
# we want to filter out the relevant DIRDEPS.*
# we should only be included by meta.autodep.mk
# if dependencies are to be updated
.for o in ${DIRDEPS_OPTIONS}
.for d in ${DIRDEPS.$o.yes} ${DIRDEPS.$o.no}
.if exists(${SRCTOP}/$d)
GENDIRDEPS_FILTER += N$d*
.elif exists(${SRCTOP}/${d:R})
GENDIRDEPS_FILTER += N${d:R}*
.endif
.endfor
.endfor
.endif
