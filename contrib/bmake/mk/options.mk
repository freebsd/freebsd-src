# $Id: options.mk,v 1.6 2013/01/28 19:28:52 sjg Exp $
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

# Inspired by FreeBSD bsd.own.mk, but intentionally simpler.

# Options are normally listed in either OPTIONS_DEFAULT_{YES,NO}
# We convert these to ${OPTION}/{yes,no} in OPTIONS_DEFAULT_VALUES.
# We add the OPTIONS_DEFAULT_NO first so they take precedence.
# This allows override of an OPTIONS_DEFAULT_YES by adding it to
# OPTIONS_DEFAULT_NO or adding ${OPTION}/no to OPTIONS_DEFAULT_VALUES.
# An OPTIONS_DEFAULT_NO option can only be overridden by putting
# ${OPTION}/yes in OPTIONS_DEFAULT_VALUES.
# A makefile may set NO_* (or NO*) to indicate it cannot do something.
# User sets WITH_* and WITHOUT_* to indicate what they want.
# We set MK_* which is then all we need care about.
OPTIONS_DEFAULT_VALUES += \
	${OPTIONS_DEFAULT_NO:O:u:S,$,/no,} \
	${OPTIONS_DEFAULT_YES:O:u:S,$,/yes,}

.for o in ${OPTIONS_DEFAULT_VALUES:M*/*}
.if ${o:T:tl} == "no"
.if defined(WITH_${o:H}) && !defined(NO_${o:H}) && !defined(NO${o:H})
MK_${o:H} ?= yes
.else
MK_${o:H} ?= no
.endif
.else
.if defined(WITHOUT_${o:H}) || defined(NO_${o:H}) || defined(NO${o:H})
MK_${o:H} ?= no
.else
MK_${o:H} ?= yes
.endif
.endif
.endfor

# OPTIONS_DEFAULT_DEPENDENT += FOO_UTILS/FOO
# if neither WITH[OUT]_FOO_UTILS is set, use value of MK_FOO
.for o in ${OPTIONS_DEFAULT_DEPENDENT:M*/*:O:u}
.if defined(WITH_${o:H}) && !defined(NO_${o:H}) && !defined(NO${o:H})
MK_${o:H} ?= yes
.elif defined(WITHOUT_${o:H}) || defined(NO_${o:H}) || defined(NO${o:H})
MK_${o:H} ?= no
.else
MK_${o:H} ?= ${MK_${o:T}}
.endif
.endfor
