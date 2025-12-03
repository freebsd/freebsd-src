# $Id: options.mk,v 1.25 2025/09/18 05:11:59 sjg Exp $
#
#	@(#) Copyright (c) 2012-2025, Simon J. Gerraty
#
#	SPDX-License-Identifier: BSD-2-Clause
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

# Inspired by FreeBSD bsd.own.mk, but intentionally simpler and more flexible.

OPTION_PREFIX ?= MK_

# Options to be forced either "yes" or "no"
OPTIONS_FORCED_VALUES += \
	${OPTIONS_BROKEN:U:O:u:S,$,/no,:N/no} \
	${OPTIONS_FORCED_NO:U:O:u:S,$,/no,:N/no} \
	${OPTIONS_FORCED_YES:U:O:u:S,$,/yes,:N/yes} \
	${OPTIONS_REQUIRED:U:O:u:S,$,/yes,:N/yes} \

.for o v in ${OPTIONS_FORCED_VALUES:M*/*:S,/, ,g}
.if !make(show-options)
.if ${v:tl} == "yes"
.if defined(WITHOUT_$o)
.warning WITHOUT_$o ignored
.endif
.elif defined(WITH_$o)
.warning WITH_$o ignored
.endif
.endif
${OPTION_PREFIX}$o := ${v:tl}
.if defined(DEBUG_OPTIONS) && ${DEBUG_OPTIONS:@x@${o:M$x}@} != ""
.info ${.INCLUDEDFROMFILE}: ${OPTION_PREFIX}$o=${${OPTION_PREFIX}$o}
.endif
.endfor

# Options are normally listed in either OPTIONS_DEFAULT_{YES,NO}
# We convert these to ${OPTION}/{yes,no} in OPTIONS_DEFAULT_VALUES.
# We add the OPTIONS_DEFAULT_NO first so they take precedence.
# This allows override of an OPTIONS_DEFAULT_YES by adding it to
# OPTIONS_DEFAULT_NO or adding ${OPTION}/no to OPTIONS_DEFAULT_VALUES.
# An OPTIONS_DEFAULT_NO option can only be overridden by putting
# ${OPTION}/yes in OPTIONS_DEFAULT_VALUES.
# A makefile may set NO_* (or NO*) to indicate it cannot do something.
# User sets WITH_* and WITHOUT_* to indicate what they want.
# We set ${OPTION_PREFIX:UMK_}* which is then all we need care about.
OPTIONS_DEFAULT_VALUES += \
	${OPTIONS_DEFAULT_NO:U:O:u:S,$,/no,:N/no} \
	${OPTIONS_DEFAULT_YES:U:O:u:S,$,/yes,:N/yes} \

# NO_* takes precedence
# If both WITH_* and WITHOUT_* are defined, WITHOUT_ wins unless
# OPTION_DOMINANT_* is set to "yes"
# Otherwise WITH_* and WITHOUT_* override the default.
.for o v in ${OPTIONS_DEFAULT_VALUES:M*/*:S,/, ,}
.if defined(WITH_$o) && ${WITH_$o:tl} == "no"
# a common miss-use - point out correct usage
.warning use WITHOUT_$o=1 not WITH_$o=no
WITHOUT_$o = 1
.endif
.if defined(NO_$o) || defined(NO$o)
# we cannot do it
${OPTION_PREFIX}$o ?= no
.elif defined(WITH_$o) && defined(WITHOUT_$o)
# normally WITHOUT_ wins
OPTION_DOMINANT_$o ?= no
${OPTION_PREFIX}$o ?= ${OPTION_DOMINANT_$o}
.elif ${v:tl} == "no"
.if defined(WITH_$o)
${OPTION_PREFIX}$o ?= yes
.else
${OPTION_PREFIX}$o ?= no
.endif
.else
.if defined(WITHOUT_$o)
${OPTION_PREFIX}$o ?= no
.else
${OPTION_PREFIX}$o ?= yes
.endif
.endif
.if defined(DEBUG_OPTIONS) && ${DEBUG_OPTIONS:@x@${o:M$x}@} != ""
.info ${.INCLUDEDFROMFILE}: ${OPTION_PREFIX}$o=${${OPTION_PREFIX}$o}
.endif
.endfor

# OPTIONS_DEFAULT_DEPENDENT += FOO_UTILS/FOO
# If neither WITH[OUT]_FOO_UTILS is set, (see rules above)
# use the value of ${OPTION_PREFIX}FOO
# Add OPTIONS_DEFAULT_DEPENDENT_REQUIRED (sans any trailing /{yes,no})
# to OPTIONS_DEFAULT_DEPENDENT to avoid the need to duplicate entries
OPTIONS_DEFAULT_DEPENDENT += ${OPTIONS_DEFAULT_DEPENDENT_REQUIRED:U:S,/yes$,,:S,/no$,,}

.for o d in ${OPTIONS_DEFAULT_DEPENDENT:M*/*:S,/, ,}
.if defined(NO_$o) || defined(NO$o)
# we cannot do it
${OPTION_PREFIX}$o ?= no
.elif defined(WITH_$o) && defined(WITHOUT_$o)
# normally WITHOUT_ wins
OPTION_DOMINANT_$o ?= no
${OPTION_PREFIX}$o ?= ${OPTION_DOMINANT_$o}
.elif defined(WITH_$o)
${OPTION_PREFIX}$o ?= yes
.elif defined(WITHOUT_$o)
${OPTION_PREFIX}$o ?= no
.else
${OPTION_PREFIX}$o ?= ${${OPTION_PREFIX}$d}
.endif
.if defined(DEBUG_OPTIONS) && ${DEBUG_OPTIONS:@x@${o:M$x}@} != ""
.info ${.INCLUDEDFROMFILE}: ${OPTION_PREFIX}$o=${${OPTION_PREFIX}$o} (${OPTION_PREFIX}$d=${${OPTION_PREFIX}$d})
.endif
.endfor

# OPTIONS_DEFAULT_DEPENDENT_REQUIRED += FOO_UTILS/FOO[/{yes,no}]
# first processed with OPTIONS_DEFAULT_DEPENDENT above,
# but if ${OPTION_PREFIX}${o:H:H} is ${o:T},
# then ${OPTION_PREFIX}${o:H:T} must be too
.for o in ${OPTIONS_DEFAULT_DEPENDENT_REQUIRED:M*/*:O:u}
# This dance allows /{yes,no} to be optional
.if ${o:T:tl:Nno:Nyes} == ""
$o.H := ${o:H:H}
$o.R := ${o:T}
$o.T := ${o:H:T}
.else
$o.H := ${o:H}
$o.R := ${OPTION_REQUIRED_${o:H}:Uyes}
$o.T := ${o:T}
.endif
.if defined(DEBUG_OPTIONS) && ${DEBUG_OPTIONS:@x@${$o.H:M$x}@} != ""
.info ${.INCLUDEDFROMFILE}: ${OPTION_PREFIX}${$o.H}=${${OPTION_PREFIX}${$o.H}} (${OPTION_PREFIX}${$o.T}=${${OPTION_PREFIX}${$o.T}} require=${$o.R})
.endif
.if ${${OPTION_PREFIX}${$o.H}} != ${${OPTION_PREFIX}${$o.T}}
.if ${${OPTION_PREFIX}${$o.H}} == ${$o.R}
.error ${OPTION_PREFIX}${$o.H}=${${OPTION_PREFIX}${$o.H}} requires ${OPTION_PREFIX}${$o.T}=${${OPTION_PREFIX}${$o.H}}
.endif
.endif
.undef $o.H
.undef $o.R
.undef $o.T
.endfor

# allow displaying/describing set options
.set_options := ${.set_options} \
	${OPTIONS_DEFAULT_VALUES:U:H:N.} \
	${OPTIONS_DEFAULT_DEPENDENT:U:H:N.} \
	${OPTIONS_FORCED_VALUES:U:H:N.} \

# this can be used in .info as well as target below
OPTIONS_SHOW ?= ${.set_options:O:u:@o@${OPTION_PREFIX}$o=${${OPTION_PREFIX}$o}@}
# prefix for variables describing options
OPTION_DESCRIPTION_PREFIX ?= DESCRIPTION_
OPTION_DESCRIPTION_SEPARATOR ?= ==

OPTIONS_DESCRIBE ?= ${.set_options:O:u:@o@${OPTION_PREFIX}$o=${${OPTION_PREFIX}$o}${${OPTION_DESCRIPTION_PREFIX}$o:S,^, ${OPTION_DESCRIPTION_SEPARATOR} ,1}${.newline}@}

.if !commands(show-options)
show-options: .NOTMAIN .PHONY
	@echo; echo "${OPTIONS_SHOW:ts\n}"; echo
.endif

.if !commands(describe-options)
describe-options: .NOTMAIN .PHONY
	@echo; echo "${OPTIONS_DESCRIBE}"; echo
.endif

# we expect to be included more than once
.undef OPTIONS_BROKEN
.undef OPTIONS_DEFAULT_DEPENDENT
.undef OPTIONS_DEFAULT_DEPENDENT_REQUIRED
.undef OPTIONS_DEFAULT_NO
.undef OPTIONS_DEFAULT_VALUES
.undef OPTIONS_DEFAULT_YES
.undef OPTIONS_FORCED_NO
.undef OPTIONS_FORCED_VALUES
.undef OPTIONS_FORCED_YES
.undef OPTIONS_REQUIRED
