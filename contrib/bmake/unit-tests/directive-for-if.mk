# $NetBSD: directive-for-if.mk,v 1.1 2021/08/30 17:08:13 rillig Exp $
#
# Test for a .for directive that contains an .if directive.
#
# Before for.c 1.39 from 2008-12-21, when expanding the variables of a .for
# loop, their values were placed verbatim in the expanded body.  Since then,
# each variable value expands to an expression of the form ${:Uvalue}.
#
# Before that change, the following adventurous code was possible:
#
#	.for directive in if ifdef ifndef
#	.  ${directive} "1" != "0"
#	.  endif
#	.endfor
#
# A more practical usage of the .for loop that often led to surprises was the
# following:
#
#	.for var in VAR1 VAR2 VAR3
#	.  if ${var} != "VAR2"
#	.  endif
#	.endfor
#
# The .for loop body expanded to this string:
#
#	.  if VAR1 != "VAR2"
#	.  endif
#
# Since bare words were not allowed at the left-hand side of a condition,
# make complained about a "Malformed conditional", which was surprising since
# the code before expanding the .for loop body looked quite well.
#
# In cond.c 1.48 from 2008-11-29, just a month before the expansion of .for
# loops changed from plain textual value to using expressions of the form
# ${:Uvalue}, this surprising behavior was documented in the code, and a
# workaround was implemented that allowed bare words when they are followed
# by either '!' or '=', as part of the operators '!=' or '=='.
#
# Since cond.c 1.68 from 2015-05-05, bare words are allowed on the left-hand
# side of a condition, but that applies only to expression of the form
# ${${cond} :? then : else}, it does not apply to conditions in ordinary .if
# directives.

# The following snippet worked in 2005, when the variables from the .for loop
# expanded to their bare textual value.
.for directive in if ifdef ifndef
.  ${directive} "1" != "0"
.  endif
.endfor
# In 2021, the above code does not generate an error message, even though the
# code looks clearly malformed.  This is due to the '!', which is interpreted
# as a dependency operator, similar to ':' and '::'.  The parser turns this
# line into a dependency with the 3 targets '.', 'if', '"1"' and the 2 sources
# '=' and '"0"'.  Since that line is not interpreted as an '.if' directive,
# the error message 'if-less endif' makes sense.

# In 2005, make complained:
#
#	.if line:	Malformed conditional (VAR1 != "VAR2")
#	.endif line:	if-less endif
#	.endif line:	Need an operator
#
# 2008.11.30.22.37.55 does not complain about the left-hand side ${var}.
.for var in VAR1 VAR2 VAR3
.  if ${var} != "VAR2"
_!=	echo "${var}" 1>&2; echo # In 2005, '.info' was not invented yet.
.  endif
.endfor

# Before for.c 1.39 from 2008-12-21, a common workaround was to surround the
# variable expression from the .for loop with '"'.  Such a string literal
# has been allowed since cond.c 1.23 from 2004-04-13.  Between that commit and
# the one from 2008, the parser would still get confused if the value from the
# .for loop contained '"', which was effectively a code injection.
#
# Surrounding ${var} with quotes disabled the check for typos though.  For
# ordinary variables, referring to an undefined variable on the left-hand side
# of the comparison resulted in a "Malformed conditional".  Since the .for
# loop was usually close to the .if clause, this was not a problem in
# practice.
.for var in VAR1 VAR2 VAR3
.  if "${var}" != "VAR2"
.  endif
.endfor

all:
