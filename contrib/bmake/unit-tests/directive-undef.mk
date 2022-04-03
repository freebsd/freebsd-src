# $NetBSD: directive-undef.mk,v 1.12 2022/03/26 12:44:57 rillig Exp $
#
# Tests for the .undef directive.
#
# See also:
#	directive-misspellings.mk

# Before var.c 1.737 from 2020-12-19, .undef only undefined the first
# variable, silently skipping all further variable names.
#
# Before var.c 1.761 from 2020-12-22, .undef complained about too many
# arguments.
#
# Since var.c 1.761 from 2020-12-22, .undef handles multiple variable names
# just like the .export directive.
1=		1
2=		2
3=		3
.undef 1 2 3
.if ${1:U_}${2:U_}${3:U_} != ___
.  warning $1$2$3
.endif


# Without any arguments, until var.c 1.736 from 2020-12-19, .undef tried
# to delete the variable with the empty name, which never exists; see
# varname-empty.mk.  Since var.c 1.737 from 2020-12-19, .undef complains
# about a missing argument.
.undef


# Trying to delete the variable with the empty name is ok, it just won't
# ever do anything since that variable is never defined.
.undef ${:U}


# The argument of .undef is first expanded exactly once and then split into
# words, just like everywhere else.  This prevents variables whose names
# contain spaces or unbalanced 'single' or "double" quotes from being
# undefined, but these characters do not appear in variables names anyway.
1=		1
2=		2
3=		3
${:U1 2 3}=	one two three
VARNAMES=	1 2 3
.undef ${VARNAMES}		# undefines the variables "1", "2" and "3"
.if ${${:U1 2 3}} != "one two three"	# still there
.  error
.endif
.if ${1:U_}${2:U_}${3:U_} != "___"	# these have been undefined
.  error
.endif


# A variable named " " cannot be undefined.  There's no practical use case
# for such variables anyway.
SPACE=		${:U }
${SPACE}=	space
.if !defined(${SPACE})
.  error
.endif
.undef ${SPACE}
.if !defined(${SPACE})
.  error
.endif


# A variable named "$" can be undefined since the argument to .undef is
# expanded exactly once, before being split into words.
DOLLAR=		$$
${DOLLAR}=	dollar
.if !defined(${DOLLAR})
.  error
.endif
.undef ${DOLLAR}
.if defined(${DOLLAR})
.  error
.endif


# Since var.c 1.762 from 2020-12-22, parse errors in the argument should be
# properly detected and should stop the .undef directive from doing any work.
#
# As of var.c 1.762, this doesn't happen though because the error handling
# in Var_Parse and Var_Subst is not done properly.
.undef ${VARNAMES:L:Z}


UT_EXPORTED=	exported-value
.export UT_EXPORTED
.if ${:!echo "\${UT_EXPORTED:-not-exported}"!} != "exported-value"
.  error
.endif
.if !${.MAKE.EXPORTED:MUT_EXPORTED}
.  error
.endif
.undef UT_EXPORTED		# XXX: does not update .MAKE.EXPORTED
.if ${:!echo "\${UT_EXPORTED:-not-exported}"!} != "not-exported"
.  error
.endif
.if ${.MAKE.EXPORTED:MUT_EXPORTED}
.  warning UT_EXPORTED is still listed in .MAKE.EXPORTED even though $\
	   it is not exported anymore.
.endif


# When an exported variable is undefined, the variable is removed both from
# the global scope as well as from the environment.
DIRECT=		direct
INDIRECT=	in-${DIRECT}
.export DIRECT INDIRECT
.if ${DIRECT} != "direct"
.  error
.endif
.if ${INDIRECT} != "in-direct"
.  error
.endif

# Deletes the variables from the global scope and also from the environment.
# This applies to both variables, even though 'INDIRECT' is not actually
# exported yet since it refers to another variable.
.undef DIRECT			# Separate '.undef' directives,
.undef INDIRECT			# for backwards compatibility.

.if ${DIRECT:Uundefined} != "undefined"
.  error
.endif
.if ${INDIRECT:Uundefined} != "undefined"
.  error
.endif


# Since var.c 1.570 from 2020-10-06 and before var.c 1.1014 from 2022-03-26,
# make ran into an assertion failure when trying to undefine a variable that
# was based on an environment variable.
.if ${ENV_VAR} != "env-value"	# see ./Makefile, ENV.directive-undef
.  error
.endif
ENV_VAR+=	appended	# moves the short-lived variable to the
				# global scope
.undef ENV_VAR			# removes the variable from both the global
				# scope and from the environment


all:
