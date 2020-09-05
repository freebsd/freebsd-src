# $NetBSD: cond-op.mk,v 1.4 2020/08/28 14:07:51 rillig Exp $
#
# Tests for operators like &&, ||, ! in .if conditions.
#
# See also:
#	cond-op-and.mk
#	cond-op-not.mk
#	cond-op-or.mk
#	cond-op-parentheses.mk

# In make, && binds more tightly than ||, like in C.
# If make had the same precedence for both && and ||, the result would be
# different.
# If || were to bind more tightly than &&, the result would be different
# as well.
.if !(1 || 1 && 0)
.error
.endif

# If make were to interpret the && and || operators like the shell, the
# implicit binding would be this:
.if (1 || 1) && 0
.error
.endif

# The precedence of the ! operator is different from C though. It has a
# lower precedence than the comparison operators.
.if !"word" == "word"
.error
.endif

# This is how the above condition is actually interpreted.
.if !("word" == "word")
.error
.endif

# TODO: Demonstrate that the precedence of the ! and == operators actually
# makes a difference.  There is a simple example for sure, I just cannot
# wrap my head around it.

# This condition is malformed because the '!' on the right-hand side must not
# appear unquoted.  If any, it must be enclosed in quotes.
# In any case, it is not interpreted as a negation of an unquoted string.
# See CondGetString.
.if "!word" == !word
.error
.endif

# Surprisingly, the ampersand and pipe are allowed in bare strings.
# That's another opportunity for writing confusing code.
# See CondGetString, which only has '!' in the list of stop characters.
.if "a&&b||c" != a&&b||c
.error
.endif

# Just in case that parsing should ever stop on the first error.
.info Parsing continues until here.

all:
	@:;
