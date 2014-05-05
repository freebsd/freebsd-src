#
# $FreeBSD$
#
# Generic mechanism to deal with WITH and WITHOUT options and turn them into MK_ options.
#
#
# For each option FOO that defaults to YES, MK_FOO is set to yes, unless WITHOUT_FOO
# is defined, in which case it is set to no. If both WITH_FOO and WITHOUT_FOO are
# defined, WITHOUT_FOO wins. The list of default yes options is contained in the
# __DEFAULT_YES_OPTIONS variable, which is undefined after expansion.
#
# For each option FOO that defaults to NO, MK_FOO is set to no, unless WITH_FOO
# is defined, in which case it is set to yes. If both WITH_FOO and WITHOUT_FOO are
# defined, WITH_FOO wins. The list of default no options is contained in the
# __DEFAULT_NO_OPTIONS variable, which is undefined after expansion.
#
.for var in ${__DEFAULT_YES_OPTIONS}
.if !defined(MK_${var})
.if defined(WITHOUT_${var})	# IF both WITH and WITHOUT defined, WITHOUT wins.
MK_${var}:=	no
.else
MK_${var}:=	yes
.endif
.endif
.endfor
.undef __DEFAULT_YES_OPTIONS

#
# MK_* options which default to "no".
#
.for var in ${__DEFAULT_NO_OPTIONS}
.if !defined(MK_${var})
.if defined(WITH_${var}) && !defined(WITHOUT_${var}) # WITHOUT wins
MK_${var}:=	yes
.else
MK_${var}:=	no
.endif
.endif
.endfor
.undef __DEFAULT_NO_OPTIONS
