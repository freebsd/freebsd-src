# $NetBSD: varname-empty.mk,v 1.9 2021/04/04 10:13:09 rillig Exp $
#
# Tests for the special variable with the empty name.
#
# There is no variable named "" at all, and this fact is used a lot in
# variable expressions of the form ${:Ufallback}.  These expressions are
# based on the variable named "" and use the :U modifier to assign a
# fallback value to the expression (but not to the variable).
#
# This form of expressions is used to implement value substitution in the
# .for loops.  Another use case is in a variable assignment of the form
# ${:Uvarname}=value, which allows for characters in the variable name that
# would otherwise be interpreted by the parser, such as whitespace, ':',
# '=', '$', backslash.
#
# The only places where a variable is assigned a value are Var_Set and
# Var_Append, and these places protect the variable named "" from being
# defined.  This is different from read-only variables, as that flag can
# only apply to variables that are defined.  The variable named "" must
# never be defined though.
#
# See also:
#	The special variables @F or ^D, in var-class-local.mk

# Until 2020-08-22 it was possible to assign a value to the variable with
# the empty name, leading to all kinds of unexpected effects in .for loops
# and other places that assume that ${:Ufallback} expands to "fallback".
# The bug in Var_Set was that only expanded variables had been checked for
# the empty name, but not the direct assignments with an empty name.
?=	default
=	assigned	# undefined behavior until 2020-08-22
+=	appended
:=	subst
!=	echo 'shell-output'
.if ${:Ufallback} != "fallback"
.  error
.endif

${:U}=	assigned indirectly
.if ${:Ufallback} != "fallback"
.  error
.endif

${:U}+=	appended indirectly
.if ${:Ufallback} != "fallback"
.  error
.endif

.MAKEFLAGS: -d0

# Before 2020-08-22, the simple assignment operator '=' after an empty
# variable name had an off-by-one bug in Parse_Var.  The code that was
# supposed to "skip to operator character" started its search _after_ the
# assignment operator, assuming that the variable name would be at least
# one character long.  It then looked for the next occurrence of a '=', which
# could be several lines away or not occur at all.  While looking for the
# '=', some whitespace was nulled out, leading to out-of-bounds write.
=	assigned	# undefined behavior until 2020-08-22

# The .for loop expands the expression ${i} to ${:U1}, ${:U2} and so on.
# This only works if the variable with the empty name is guaranteed to
# be undefined.
.for i in 1 2 3
NUMBERS+=	${i}
.endfor

all:
	@echo out: ${:Ufallback}
	@echo out: ${NUMBERS}
