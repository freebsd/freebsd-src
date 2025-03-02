#
#
# Generic mechanism to deal with WITH and WITHOUT options and turn
# them into MK_ options.  Also turn group options into OPT_ options.
#
# For each option FOO in __DEFAULT_YES_OPTIONS, MK_FOO is set to
# "yes", unless WITHOUT_FOO is defined, in which case it is set to
# "no".
#
# For each option FOO in __REQUIRED_OPTIONS, MK_FOO is set to "yes".
#
# For each option FOO in __DEFAULT_NO_OPTIONS, MK_FOO is set to "no",
# unless WITH_FOO is defined, in which case it is set to "yes".
#
# For each entry FOO/BAR in __DEFAULT_DEPENDENT_OPTIONS,
# MK_FOO is set to "no" if WITHOUT_FOO is defined,
# "yes" if WITH_FOO is defined, otherwise the value of MK_BAR.
#
# If both WITH_FOO and WITHOUT_FOO are defined, WITHOUT_FOO wins and
# MK_FOO is set to "no" regardless of which list it was in.
#
# All of __DEFAULT_YES_OPTIONS, __DEFAULT_NO_OPTIONS and
# __DEFAULT_DEPENDENT_OPTIONS are undef'd after all this processing,
# allowing this file to be included multiple times with different lists.
#
# Other parts of the build system will set BROKEN_OPTIONS to a list
# of options that are broken on this platform. This will not be unset
# before returning. Clients are expected to always += this variable.
#
# Users should generally define WITH_FOO or WITHOUT_FOO, but the build
# system should use MK_FOO={yes,no} when it needs to override the
# user's desires or default behavior.
#
# For each option in __SINGLE_OPTIONS, OPT_FOO is set to FOO if
# defined and __FOO_DEFAULT if not.  Valid values for FOO are specified
# by __FOO_OPTIONS.
#
# Other parts of the build system will set BROKEN_SINGLE_OPTIONS to a
# list of 3-tuples of the form: "OPTION broken_value replacment_value".
# This will not be unset before returning. Clients are expected to
# always += this variable.
#

#
# MK_* options which default to "yes".
#
.for var in ${__DEFAULT_YES_OPTIONS}
.if !defined(MK_${var})
.if defined(WITH_${var}) && ${WITH_${var}} == "no"
.warning Use WITHOUT_${var}=1 instead of WITH_${var}=no
.endif
.if defined(WITHOUT_${var})			# WITHOUT always wins
MK_${var}:=	no
.else
MK_${var}:=	yes
.endif
.else
.if ${MK_${var}} != "yes" && ${MK_${var}} != "no"
.error Illegal value for MK_${var}: ${MK_${var}}
.endif
.endif # !defined(MK_${var})
.endfor
.undef __DEFAULT_YES_OPTIONS

#
# MK_* options which are always yes, typically as a transitional
# step towards removing the options entirely.
#
.for var in ${__REQUIRED_OPTIONS}
.if defined(WITHOUT_${var}) && !make(showconfig)
.warning WITHOUT_${var} option ignored: it is no longer supported
.endif
MK_${var}:=	yes
.endfor

#
# MK_* options which default to "no".
#
.for var in ${__DEFAULT_NO_OPTIONS}
.if !defined(MK_${var})
.if defined(WITH_${var}) && ${WITH_${var}} == "no"
.warning Use WITHOUT_${var}=1 instead of WITH_${var}=no
.endif
.if defined(WITH_${var}) && !defined(WITHOUT_${var}) # WITHOUT always wins
MK_${var}:=	yes
.else
MK_${var}:=	no
.endif
.else
.if ${MK_${var}} != "yes" && ${MK_${var}} != "no"
.error Illegal value for MK_${var}: ${MK_${var}}
.endif
.endif # !defined(MK_${var})
.endfor
.undef __DEFAULT_NO_OPTIONS

#
# MK_* options which are always no, usually because they are
# unsupported/badly broken on this architecture.
#
.for var in ${BROKEN_OPTIONS}
MK_${var}:=	no
.endfor

#
# Group options set an OPT_FOO variable for each option.
#
.for opt in ${__SINGLE_OPTIONS}
.if !defined(__${opt}_OPTIONS) || empty(__${opt}_OPTIONS)
.error __${opt}_OPTIONS undefined or empty
.endif
.if !defined(__${opt}_DEFAULT) || empty(__${opt}_DEFAULT)
.error __${opt}_DEFAULT undefined or empty
.endif
.if defined(${opt})
OPT_${opt}:=	${${opt}}
.else
OPT_${opt}:=	${__${opt}_DEFAULT}
.endif
.if empty(OPT_${opt}) || ${__${opt}_OPTIONS:M${OPT_${opt}}} != ${OPT_${opt}}
.error Invalid option OPT_${opt} (${OPT_${opt}}), must be one of: ${__${opt}_OPTIONS}
.endif
.endfor
.undef __SINGLE_OPTIONS

.for opt val rep in ${BROKEN_SINGLE_OPTIONS}
.if ${OPT_${opt}} == ${val}
OPT_${opt}:=    ${rep}
.endif
.endfor

.for vv in ${__DEFAULT_DEPENDENT_OPTIONS}
.if defined(WITH_${vv:H}) && defined(WITHOUT_${vv:H})
MK_${vv:H}?= no
.elif defined(WITH_${vv:H})
MK_${vv:H}?= yes
.elif defined(WITHOUT_${vv:H})
MK_${vv:H}?= no
.else
MK_${vv:H}?= ${MK_${vv:T}}
.endif
MK_${vv:H}:= ${MK_${vv:H}}
.endfor
.undef __DEFAULT_DEPENDENT_OPTIONS
