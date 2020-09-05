# $NetBSD: directive-for.mk,v 1.2 2020/09/02 22:58:59 rillig Exp $
#
# Tests for the .for directive.

# Using the .for loop, lists of values can be produced.
# In simple cases, the :@var@${var}@ variable modifier can be used to
# reach the same effects.
#
.undef NUMBERS
.for num in 1 2 3
NUMBERS+=	${num}
.endfor
.if ${NUMBERS} != "1 2 3"
.  error
.endif

# The .for loop also works for multiple iteration variables.
.for name value in VARNAME value NAME2 value2
${name}=	${value}
.endfor
.if ${VARNAME} != "value" || ${NAME2} != "value2"
.  error
.endif

# The .for loop splits the items at whitespace, taking quotes into account,
# just like the :M or :S variable modifiers.
#
# Until 2012-06-03, it had split the items exactly at whitespace, without
# taking the quotes into account.
#
.undef WORDS
.for var in one t\ w\ o "three three" 'four four' `five six`
WORDS+=	counted
.endfor
.if ${WORDS:[#]} != 6
.  error
.endif

# In the body of the .for loop, the iteration variables can be accessed
# like normal variables, even though they are not really variables.
#
# Instead, the expression ${var} is transformed into ${:U1}, ${:U2} and so
# on, before the loop body is evaluated.
#
# A notable effect of this implementation technique is that the .for
# iteration variables and the normal global variables live in separate
# namespaces and do not influence each other.
#
var=	value before
var2=	value before
.for var var2 in 1 2 3 4
.endfor
.if ${var} != "value before"
.  warning After the .for loop, var must still have its original value.
.endif
.if ${var2} != "value before"
.  warning After the .for loop, var2 must still have its original value.
.endif

# Everything from the paragraph above also applies if the loop body is
# empty, even if there is no actual iteration since the loop items are
# also empty.
#
var=	value before
var2=	value before
.for var var2 in ${:U}
.endfor
.if ${var} != "value before"
.  warning After the .for loop, var must still have its original value.
.endif
.if ${var2} != "value before"
.  warning After the .for loop, var2 must still have its original value.
.endif

# Until 2008-12-21, the values of the iteration variables were simply
# inserted as plain text and then parsed as usual, which made it possible
# to achieve all kinds of strange effects.
#
# Before that date, the .for loop expanded to:
#	EXPANSION+= value
# Since that date, the .for loop expands to:
#	EXPANSION${:U+}= value
#
EXPANSION=	before
EXPANSION+ =	before
.for plus in +
EXPANSION${plus}=	value
.endfor
.if ${EXPANSION} != "before"
.  error This must be a make from before 2009.
.endif
.if ${EXPANSION+} != "value"
.  error This must be a make from before 2009.
.endif

all:
	@:;
