#
# $FreeBSD$
#
# Generic mechanism to deal with WITH and WITHOUT options and turn
# them into MK_ options.
#
# For each option FOO in __DEFAULT_YES_OPTIONS, MK_FOO is set to
# "yes", unless WITHOUT_FOO is defined, in which case it is set to
# "no".
#
# For each option FOO in __DEFAULT_NO_OPTIONS, MK_FOO is set to "no",
# unless WITH_FOO is defined, in which case it is set to "yes".
#
# If both WITH_FOO and WITHOUT_FOO are defined, WITHOUT_FOO wins and
# MK_FOO is set to "no" regardless of which list it was in.
#
# Both __DEFAULT_YES_OPTIONS and __DEFAULT_NO_OPTIONS are undef'd
# after all this processing, allowing this file to be included
# multiple times with different lists.
#
# Users should generally define WITH_FOO or WITHOUT_FOO, but the build
# system should use MK_FOO={yes,no} when it needs to override the
# user's desires or default behavior.
#

#
# MK_* options which default to "yes".
#
.for var in ${__DEFAULT_YES_OPTIONS}
.if !defined(MK_${var})
.if defined(WITHOUT_${var})			# WITHOUT always wins
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
.if defined(WITH_${var}) && !defined(WITHOUT_${var}) # WITHOUT always wins
MK_${var}:=	yes
.else
MK_${var}:=	no
.endif
.endif
.endfor
.undef __DEFAULT_NO_OPTIONS
