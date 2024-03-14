# SPDX-License-Identifier: BSD-2-Clause
#
# $Id: options.mk,v 1.20 2024/02/17 17:26:57 sjg Exp $
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

# Inspired by FreeBSD bsd.own.mk, but intentionally simpler and more flexible.

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
	${OPTIONS_DEFAULT_NO:U:O:u:S,$,/no,} \
	${OPTIONS_DEFAULT_YES:U:O:u:S,$,/yes,}

OPTION_PREFIX ?= MK_

# NO_* takes precedence
# If both WITH_* and WITHOUT_* are defined, WITHOUT_ wins unless
# DOMINANT_* is set to "yes"
# Otherwise WITH_* and WITHOUT_* override the default.
.for o in ${OPTIONS_DEFAULT_VALUES:M*/*}
.if defined(WITH_${o:H}) && ${WITH_${o:H}} == "no"
# a common miss-use - point out correct usage
.warning use WITHOUT_${o:H}=1 not WITH_${o:H}=no
.endif
.if defined(NO_${o:H}) || defined(NO${o:H})
# we cannot do it
${OPTION_PREFIX}${o:H} ?= no
.elif defined(WITH_${o:H}) && defined(WITHOUT_${o:H})
# normally WITHOUT_ wins
DOMINANT_${o:H} ?= no
${OPTION_PREFIX}${o:H} ?= ${DOMINANT_${o:H}}
.elif ${o:T:tl} == "no"
.if defined(WITH_${o:H})
${OPTION_PREFIX}${o:H} ?= yes
.else
${OPTION_PREFIX}${o:H} ?= no
.endif
.else
.if defined(WITHOUT_${o:H})
${OPTION_PREFIX}${o:H} ?= no
.else
${OPTION_PREFIX}${o:H} ?= yes
.endif
.endif
.endfor

# OPTIONS_DEFAULT_DEPENDENT += FOO_UTILS/FOO
# If neither WITH[OUT]_FOO_UTILS is set, (see rules above)
# use the value of ${OPTION_PREFIX}FOO
.for o in ${OPTIONS_DEFAULT_DEPENDENT:M*/*:O:u}
.if defined(NO_${o:H}) || defined(NO${o:H})
# we cannot do it
${OPTION_PREFIX}${o:H} ?= no
.elif defined(WITH_${o:H}) && defined(WITHOUT_${o:H})
# normally WITHOUT_ wins
DOMINANT_${o:H} ?= no
${OPTION_PREFIX}${o:H} ?= ${DOMINANT_${o:H}}
.elif defined(WITH_${o:H})
${OPTION_PREFIX}${o:H} ?= yes
.elif defined(WITHOUT_${o:H})
${OPTION_PREFIX}${o:H} ?= no
.else
${OPTION_PREFIX}${o:H} ?= ${${OPTION_PREFIX}${o:T}}
.endif
.endfor

# allow displaying/describing set options
.set_options := ${.set_options} \
	${OPTIONS_DEFAULT_VALUES:H:N.} \
	${OPTIONS_DEFAULT_DEPENDENT:U:H:N.} \

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
.undef OPTIONS_DEFAULT_DEPENDENT
.undef OPTIONS_DEFAULT_NO
.undef OPTIONS_DEFAULT_VALUES
.undef OPTIONS_DEFAULT_YES
