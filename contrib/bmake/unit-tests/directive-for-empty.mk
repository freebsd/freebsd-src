# $NetBSD: directive-for-empty.mk,v 1.3 2023/11/19 21:47:52 rillig Exp $
#
# Tests for .for loops containing conditions of the form 'empty(var:...)'.
#
# When a .for loop is expanded, expressions in the body of the loop
# are replaced with expressions containing the variable values.  This
# replacement is a bit naive but covers most of the practical cases.  The one
# popular exception is the condition 'empty(var:Modifiers)', which does not
# look like an expression and is thus not replaced.
#
# See also:
#	https://gnats.netbsd.org/43821


# In the body of the .for loop, the expression '${i:M*2*}' is replaced with
# '${:U11:M*2*}', '${:U12:M*2*}', '${:U13:M*2*}', one after another.  This
# replacement creates the impression that .for variables were real variables,
# when in fact they aren't.
.for i in 11 12 13
.  if ${i:M*2*}
# expect+1: 2
.info 2
.  endif
.endfor


# In conditions, the function call to 'empty' does not look like an
# expression, therefore it is not replaced.  Since there is no global variable
# named 'i', this expression makes for a leaky abstraction.  If the .for
# variables were real variables, calling 'empty' would work on them as well.
.for i in 11 12 13
# Asking for an empty iteration variable does not make sense as the .for loop
# splits the iteration items into words, and such a word cannot be empty.
.  if empty(i)
# expect+3: Missing argument for ".error"
# expect+2: Missing argument for ".error"
# expect+1: Missing argument for ".error"
.    error			# due to the leaky abstraction
.  endif
# The typical way of using 'empty' with variables from .for loops is pattern
# matching using the modifiers ':M' or ':N'.
.  if !empty(i:M*2*)
.    if ${i} != "12"
.      error
.    endif
.  endif
.endfor


# The idea of replacing every occurrences of 'empty(i' in the body of a .for
# loop would be naive and require many special cases, as there are many cases
# that need to be considered when deciding whether the token 'empty' is a
# function call or not, as demonstrated by the following examples.  For
# expressions like '${i:Modifiers}', this is simpler as a single
# dollar almost always starts an expression.  For counterexamples and
# edge cases, see directive-for-escape.mk.  Adding another such tricky detail
# is out of the question.
.MAKEFLAGS: -df
.for i in value
# The identifier 'empty' can only be used in conditions such as .if, .ifdef or
# .elif.  In other lines the string 'empty(' must be preserved.
CPPFLAGS+=	-Dmessage="empty(i)"
# There may be whitespace between 'empty' and '('.
.if ! empty (i)
.  error
.endif
# Even in conditions, the string 'empty(' is not always a function call, it
# can occur in a string literal as well.
.if "empty\(i)" != "empty(i)"
.  error
.endif
# In comments like 'empty(i)', the text must be preserved as well.
#
# Conditions, including function calls to 'empty', can not only occur in
# condition directives, they can also occur in the modifier ':?', see
# varmod-ifelse.mk.
CPPFLAGS+=	-Dmacro="${empty(i):?empty:not-empty}"
.endfor
.MAKEFLAGS: -d0


# An idea to work around the above problems is to collect the variables from
# the .for loops in a separate scope.  To match the current behavior, there
# has to be one scope per included file.  There may be .for loops using the
# same variable name in files that include each other:
#
# outer.mk:	.for i in outer
#		.  info $i		# outer
#		.  include "inner.mk"
# inner.mk:	.    info $i		# (undefined)
#		.    for i in inner
#		.      info $i		# inner
#		.    endfor
#		.    info $i		# (undefined)
# outer.mk:	.  info $i		# outer
#		.endfor
#
# This might be regarded another leaky abstraction, but it is in fact useful
# that variables from .for loops can only affect expressions in the current
# file.  If variables from .for loops were implemented as global variables,
# they might interact between files.
#
# To emulate this exact behavior for the function 'empty', each file in the
# stack of included files needs its own scope that is independent from the
# other files.
#
# Another tricky detail are nested .for loops in a single file that use the
# same variable name.  These are generally avoided by developers, as they
# would be difficult to understand for humans as well.  Technically, they are
# possible though.  Assuming there are two nested .for loops, both using the
# variable 'i'.  When the inner .for loop ends, the inner 'i' needs to be
# removed from the scope, which would need to make the outer 'i' visible
# again.  This would suggest to use one variable scope per .for loop.
#
# Using a separate scope has the benefit that Var_Parse already allows for
# a custom scope to be passed as parameter.  This would have another side
# effect though.  There are several modifiers that actually modify variables,
# and these modifications happen in the scope that is passed to Var_Parse.
# This would mean that the combination of a .for variable and the modifiers
# '::=', '::+=', '::?=', '::!=' and ':_' would lead to different behavior than
# before.

# TODO: Add code that demonstrates the current interaction between variables
#  from .for loops and the modifiers mentioned above.
