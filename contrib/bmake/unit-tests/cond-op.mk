# $NetBSD: cond-op.mk,v 1.13 2021/01/19 18:20:30 rillig Exp $
#
# Tests for operators like &&, ||, ! in .if conditions.
#
# See also:
#	cond-op-and.mk
#	cond-op-not.mk
#	cond-op-or.mk
#	cond-op-parentheses.mk

# In make, && binds more tightly than ||, like in C.
# If make had the same precedence for both && and ||, like in the shell,
# the result would be different.
# If || were to bind more tightly than &&, the result would be different
# as well.
.if !(1 || 1 && 0)
.  error
.endif

# If make were to interpret the && and || operators like the shell, the
# previous condition would be interpreted as:
.if (1 || 1) && 0
.  error
.endif

# The precedence of the ! operator is different from C though. It has a
# lower precedence than the comparison operators.  Negating a condition
# does not need parentheses.
#
# This kind of condition looks so unfamiliar that it doesn't occur in
# practice.
.if !"word" == "word"
.  error
.endif

# This is how the above condition is actually interpreted.
.if !("word" == "word")
.  error
.endif

# TODO: Demonstrate that the precedence of the ! and == operators actually
# makes a difference.  There is a simple example for sure, I just cannot
# wrap my head around it right now.  See the truth table generator below
# for an example that doesn't require much thought.

# This condition is malformed because the '!' on the right-hand side must not
# appear unquoted.  If any, it must be enclosed in quotes.
# In any case, it is not interpreted as a negation of an unquoted string.
# See CondParser_String.
.if "!word" == !word
.  error
.endif

# Surprisingly, the ampersand and pipe are allowed in bare strings.
# That's another opportunity for writing confusing code.
# See CondParser_String, which only has '!' in the list of stop characters.
.if "a&&b||c" != a&&b||c
.  error
.endif

# As soon as the parser sees the '$', it knows that the condition will
# be malformed.  Therefore there is no point in evaluating it.
#
# As of 2021-01-20, that part of the condition is evaluated nevertheless,
# since CondParser_Or just requests the next token, without restricting
# the token to the expected tokens.  If the parser were to restrict the
# valid follow tokens for the token "0" to those that can actually produce
# a correct condition (which in this case would be comparison operators,
# TOK_AND, TOK_OR or TOK_RPAREN), the variable expression would not have
# to be evaluated.
#
# This would add a good deal of complexity to the code though, for almost
# no benefit, especially since most expressions and conditions are side
# effect free.
.if 0 ${ERR::=evaluated}
.  error
.endif
.if ${ERR:Uundefined} == evaluated
.  info After detecting a parse error, the rest is evaluated.
.endif

# Just in case that parsing should ever stop on the first error.
.info Parsing continues until here.

# Demonstration that '&&' has higher precedence than '||'.
.info A B C   =>   (A || B) && C   A || B && C   A || (B && C)
.for a in 0 1
.  for b in 0 1
.    for c in 0 1
.      for r1 in ${ ($a || $b) && $c :?1:0}
.        for r2 in ${ $a || $b && $c :?1:0}
.          for r3 in ${ $a || ($b && $c) :?1:0}
.            info $a $b $c   =>   ${r1}               ${r2}             ${r3}
.          endfor
.        endfor
.      endfor
.    endfor
.  endfor
.endfor

# This condition is obviously malformed.  It is properly detected and also
# was properly detected before 2021-01-19, but only because the left hand
# side of the '&&' evaluated to true.
.if 1 &&
.  error
.else
.  error
.endif

# This obviously malformed condition was not detected as such before cond.c
# 1.238 from 2021-01-19.
.if 0 &&
.  error
.else
.  error
.endif

# This obviously malformed condition was not detected as such before cond.c
# 1.238 from 2021-01-19.
.if 1 ||
.  error
.else
.  error
.endif

# This condition is obviously malformed.  It is properly detected and also
# was properly detected before 2021-01-19, but only because the left hand
# side of the '||' evaluated to false.
.if 0 ||
.  error
.else
.  error
.endif

all:
	@:;
