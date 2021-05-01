# $NetBSD: var-op-default.mk,v 1.3 2020/12/07 21:35:43 rillig Exp $
#
# Tests for the ?= variable assignment operator, which only assigns
# if the variable is still undefined.

# The variable VAR is not defined yet.  Therefore it gets the default value
# from the variable assignment.
VAR?=		default value
.if ${VAR} != "default value"
.  error
.endif

# At this point, the variable 'VAR' is already defined.  The '?=' therefore
# ignores the new variable value, preserving the previous "default value".
VAR?=		ignored
.if ${VAR} != "default value"
.  error
.endif

# The '?=' operator only checks whether the variable is defined or not.
# An empty variable is defined, therefore the '?=' operator does nothing.
EMPTY=		# empty
EMPTY?=		ignored
.if ${EMPTY} != ""
.  error
.endif

# The .for loop is described in the manual page as if it would operate on
# variables.  This is not entirely true.  Instead, each occurrence of an
# expression $i or ${i} or ${i:...} is substituted with ${:Uloop-value}.
# This comes very close to the description, the only difference is that
# there is never an actual variable named 'i' involved.
#
# Because there is not really a variable named 'i', the '?=' operator
# performs the variable assignment, resulting in $i == "default".
.for i in loop-value
i?=		default
.endfor
.if ${i} != "default"
.  error
.endif

# At the point where the '?=' operator checks whether the variable exists,
# it expands the variable name exactly once.  Therefore both 'VAR.param'
# and 'VAR.${param}' expand to 'VAR.param', and the second '?=' assignment
# has no effect.
#
# Since 2000.05.11.07.43.42 it has been possible to use nested variable
# expressions in variable names, which made make much more versatile.
# On 2008.03.31.00.12.21, this particular case of the '?=' operator has been
# fixed.  Before, the '?=' operator had not expanded the variable name
# 'VAR.${:Uparam}' to see whether the variable already existed.  Since that
# variable didn't exist (and variables with '$' in their name are particularly
# fragile), the variable assignment with "not used" was performed, and only
# during that, the variable name was expanded.
VAR.param=		already defined
VAR.${:Uparam}?=	not used
.if ${VAR.param} != "already defined"
.  error
.endif

# Now demonstrate that the variable name is indeed expanded exactly once.
# This is tricky to measure correctly since there are many inconsistencies
# in and around the code that expands variable expressions in the various
# places where variable expressions can occur.  If in doubt, enable the
# following debug flags to see what happens:
#.MAKEFLAGS: -dcpv
EXPAND_NAME=		EXPAND.$$$$	# The full variable name is EXPAND.$$
PARAM=			$$$$
EXPAND.${PARAM}?=	value with param
.if ${${EXPAND_NAME}} != "value with param"
.  error
.endif
.MAKEFLAGS: -d0

all:
	@:;
