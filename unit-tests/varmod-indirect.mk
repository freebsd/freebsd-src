# $NetBSD: varmod-indirect.mk,v 1.5 2020/12/27 17:32:25 rillig Exp $
#
# Tests for indirect variable modifiers, such as in ${VAR:${M_modifiers}}.
# These can be used for very basic purposes like converting a string to either
# uppercase or lowercase, as well as for fairly advanced modifiers that first
# look like line noise and are hard to decipher.
#
# TODO: Since when are indirect modifiers supported?


# To apply a modifier indirectly via another variable, the whole
# modifier must be put into a single variable expression.
.if ${value:L:${:US}${:U,value,replacement,}} != "S,value,replacement,}"
.  warning unexpected
.endif


# Adding another level of indirection (the 2 nested :U expressions) helps.
.if ${value:L:${:U${:US}${:U,value,replacement,}}} != "replacement"
.  warning unexpected
.endif


# Multiple indirect modifiers can be applied one after another as long as
# they are separated with colons.
.if ${value:L:${:US,a,A,}:${:US,e,E,}} != "vAluE"
.  warning unexpected
.endif


# An indirect variable that evaluates to the empty string is allowed though.
# This makes it possible to define conditional modifiers, like this:
#
# M.little-endian=	S,1234,4321,
# M.big-endian=		# none
.if ${value:L:${:Dempty}S,a,A,} != "vAlue"
.  warning unexpected
.endif


# The nested variable expression expands to "tu", and this is interpreted as
# a variable modifier for the value "Upper", resulting in "UPPER".
.if ${Upper:L:${:Utu}} != "UPPER"
.  error
.endif

# The nested variable expression expands to "tl", and this is interpreted as
# a variable modifier for the value "Lower", resulting in "lower".
.if ${Lower:L:${:Utl}} != "lower"
.  error
.endif


# The nested variable expression is ${1 != 1:?Z:tl}, consisting of the
# condition "1 != 1", the then-branch "Z" and the else-branch "tl".  Since
# the condition evaluates to false, the then-branch is ignored (it would
# have been an unknown modifier anyway) and the ":tl" modifier is applied.
.if ${Mixed:L:${1 != 1:?Z:tl}} != "mixed"
.  error
.endif


# The indirect modifier can also replace an ':L' modifier, which allows for
# brain twisters since by reading the expression alone, it is not possible
# to say whether the variable name will be evaluated as a variable name or
# as the immediate value of the expression.
VAR=	value
M_ExpandVar=	# an empty modifier
M_VarAsValue=	L
#
.if ${VAR:${M_ExpandVar}} != "value"
.  error
.endif
.if ${VAR:${M_VarAsValue}} != "VAR"
.  error
.endif

# The indirect modifier M_ListToSkip, when applied to a list of patterns,
# expands to a sequence of ':N' modifiers, each of which filters one of the
# patterns.  This list of patterns can then be applied to another variable
# to actually filter that variable.
#
M_ListToSkip=	@pat@N$${pat}@:ts:
#
# The dollar signs need to be doubled in the above modifier expression,
# otherwise they would be expanded too early, that is, when parsing the
# modifier itself.
#
# In the following example, M_NoPrimes expands to 'N2:N3:N5:N7:N1[1379]'.
# The 'N' comes from the expression 'N${pat}', the separating colons come
# from the modifier ':ts:'.
#
#.MAKEFLAGS: -dcv		# Uncomment this line to see the details
#
PRIMES=		2 3 5 7 1[1379]
M_NoPrimes=	${PRIMES:${M_ListToSkip}}
.if ${:U:range=20:${M_NoPrimes}} != "1 4 6 8 9 10 12 14 15 16 18 20"
.  error
.endif
.MAKEFLAGS: -d0


# In contrast to the .if conditions, the .for loop allows undefined variable
# expressions.  These expressions expand to empty strings.

# An undefined expression without any modifiers expands to an empty string.
.for var in before ${UNDEF} after
.  info ${var}
.endfor

# An undefined expression with only modifiers that keep the expression
# undefined expands to an empty string.
.for var in before ${UNDEF:${:US,a,a,}} after
.  info ${var}
.endfor

# Even in an indirect modifier based on an undefined variable, the value of
# the expression in Var_Parse is a simple empty string.
.for var in before ${UNDEF:${:U}} after
.  info ${var}
.endfor

# An error in an indirect modifier.
.for var in before ${UNDEF:${:UZ}} after
.  info ${var}
.endfor


# Another slightly different evaluation context is the right-hand side of
# a variable assignment using ':='.
.MAKEFLAGS: -dpv

# The undefined variable expression is kept as-is.
_:=	before ${UNDEF} after

# The undefined variable expression is kept as-is.
_:=	before ${UNDEF:${:US,a,a,}} after

# XXX: The subexpression ${:U} is fully defined, therefore it is expanded.
# This results in ${UNDEF:}, which can lead to tricky parse errors later,
# when the variable '_' is expanded further.
#
# XXX: What should be the correct strategy here?  One possibility is to
# expand the defined subexpression and replace it with ${:U...}, just like
# in .for loops.  This would preserve the structure of the expression while
# at the same time expanding the expression as far as possible.
_:=	before ${UNDEF:${:U}} after

# XXX: This expands to ${UNDEF:Z}, which will behave differently if the
# variable '_' is used in a context where the variable expression ${_} is
# parsed but not evaluated.
_:=	before ${UNDEF:${:UZ}} after

.MAKEFLAGS: -d0
.undef _

all:
