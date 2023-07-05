# $NetBSD: directive-for-errors.mk,v 1.6 2023/06/01 20:56:35 rillig Exp $
#
# Tests for error handling in .for loops.


# A .for directive must be followed by whitespace, everything else results
# in a parse error.
# expect+1: Unknown directive "fori"
.fori in 1 2 3
.  warning <${i}>
.endfor
# expect-2: <>
# expect-2: for-less endfor


# A slash is not whitespace, therefore this is not parsed as a .for loop.
#
# XXX: The error message is misleading though.  As of 2020-12-31, it says
# 'Unknown directive "for"', but that directive is actually known.  This is
# because ForEval does not detect the .for loop as such, so parsing
# continues in ParseLine > ParseDependencyLine > ParseDependency >
# ParseDependencyTargets > ParseErrorNoDependency, and there the directive
# name is parsed a bit differently.
# expect+1: Unknown directive "for"
.for/i in 1 2 3
.  warning <${i}>
.endfor
# expect-2: warning: <>
# expect-2: for-less endfor


# Before for.c 1.173 from 2023-05-08, the variable name could be an arbitrary
# word, it only needed to be separated by whitespace.  Even '$' and '\' were
# valid variable names, which was not useful in practice.
#
# The '$$' was not replaced with the values '1' or '3' from the .for loop,
# instead it was kept as-is, and when the .info directive expanded its
# argument, each '$$' got replaced with a single '$'.  The "long variable
# expression" ${$} got replaced though, even though this would be a parse
# error everywhere outside a .for loop.
${:U\$}=	dollar		# see whether the "variable" '$' is local
${:U\\}=	backslash	# see whether the "variable" '\' is local
# expect+1: invalid character '$' in .for loop variable name
.for $ \ in 1 2 3 4
.  info Dollar $$ ${$} $($) and backslash $\ ${\} $(\).
.endfor

# If there are no variables, there is no point in expanding the .for loop
# since this would end up in an endless loop, consuming 0 of the 3 values in
# each iteration.
# expect+1: no iteration variables in for
.for in 1 2 3
# XXX: This should not be reached.  It should be skipped, as already done
# when the number of values is not a multiple of the number of variables,
# see below.
.  warning Should not be reached.
.endfor


# There are 3 variables and 5 values.  These 5 values cannot be split evenly
# among the variables, therefore the loop is not expanded at all, it is
# skipped instead.
# expect+1: Wrong number of words (5) in .for substitution list with 3 variables
.for a b c in 1 2 3 4 5
.  warning Should not be reached.
.endfor


# The list of values after the 'in' may be empty, no matter if this emptiness
# comes from an empty expansion or even from a syntactically empty line.
.for i in
.  info Would be reached if there were items to loop over.
.endfor


# A missing 'in' should parse the .for loop but skip the body.
# expect+1: missing `in' in for
.for i over k
# XXX: As of 2020-12-31, this line is reached once.
.  warning Should not be reached.
.endfor


# A malformed modifier should be detected and skip the body of the loop.
#
# XXX: As of 2020-12-31, Var_Subst doesn't report any errors, therefore
# the loop body is expanded as if no error had happened.
# expect+1: Unknown modifier "Z"
.for i in 1 2 ${:U3:Z} 4
.  warning Should not be reached.
.endfor
# expect-2: Should not be reached.
# expect-3: Should not be reached.
# expect-4: Should not be reached.
