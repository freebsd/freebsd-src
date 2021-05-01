# $NetBSD: directive-undef.mk,v 1.9 2020/12/22 20:10:21 rillig Exp $
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
.undef ${VARNAMES}		# undefines the variable "1 2 3"
.if !defined(${:U1 2 3})
.  error
.endif
.if ${1:U_}${2:U_}${3:U_} != "___"	# these are still defined
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


all:
	@:;
