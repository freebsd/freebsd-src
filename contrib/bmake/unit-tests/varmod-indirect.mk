# $NetBSD: varmod-indirect.mk,v 1.11 2022/01/15 12:35:18 rillig Exp $
#
# Tests for indirect variable modifiers, such as in ${VAR:${M_modifiers}}.
# These can be used for very basic purposes like converting a string to either
# uppercase or lowercase, as well as for fairly advanced modifiers that first
# look like line noise and are hard to decipher.
#
# Initial support for indirect modifiers was added in var.c 1.101 from
# 2006-02-18.  Since var.c 1.108 from 2006-05-11 it is possible to use
# indirect modifiers for all but the very first modifier as well.


# To apply a modifier indirectly via another variable, the whole
# modifier must be put into a single variable expression.
# The following expression generates a parse error since its indirect
# modifier contains more than a sole variable expression.
#
# expect+1: Unknown modifier "${"
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


# An indirect variable that evaluates to the empty string is allowed.
# It is even allowed to write another modifier directly afterwards.
# There is no practical use case for this feature though, as demonstrated
# in the test case directly below.
.if ${value:L:${:Dempty}S,value,replaced,} != "replaced"
.  warning unexpected
.endif

# If an expression for an indirect modifier evaluates to anything else than an
# empty string and is neither followed by a ':' nor '}', this produces a parse
# error.  Because of this parse error, this feature cannot be used reasonably
# in practice.
#
# expect+2: Unknown modifier "${"
#.MAKEFLAGS: -dvc
.if ${value:L:${:UM*}S,value,replaced,} == "M*S,value,replaced,}"
.  warning	FIXME: this expression should have resulted in a parse $\
 		error rather than returning the unparsed portion of the $\
 		expression.
.else
.  error
.endif
#.MAKEFLAGS: -d0

# An indirect modifier can be followed by other modifiers, no matter if the
# indirect modifier evaluates to an empty string or not.
#
# This makes it possible to define conditional modifiers, like this:
#
# M.little-endian=	S,1234,4321,
# M.big-endian=		# none
.if ${value:L:${:D empty }:S,value,replaced,} != "replaced"
.  error
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


# When evaluating indirect modifiers, these modifiers may expand to ':tW',
# which modifies the interpretation of the expression value. This modified
# interpretation only lasts until the end of the indirect modifier, it does
# not influence the outer variable expression.
.if ${1 2 3:L:tW:[#]} != 1		# direct :tW applies to the :[#]
.  error
.endif
.if ${1 2 3:L:${:UtW}:[#]} != 3		# indirect :tW does not apply to :[#]
.  error
.endif


# When evaluating indirect modifiers, these modifiers may expand to ':ts*',
# which modifies the interpretation of the expression value. This modified
# interpretation only lasts until the end of the indirect modifier, it does
# not influence the outer variable expression.
#
# In this first expression, the direct ':ts*' has no effect since ':U' does not
# treat the expression value as a list of words but as a single word.  It has
# to be ':U', not ':D', since the "expression name" is "1 2 3" and there is no
# variable of that name.
#.MAKEFLAGS: -dcpv
.if ${1 2 3:L:ts*:Ua b c} != "a b c"
.  error
.endif
# In this expression, the direct ':ts*' affects the ':M' at the end.
.if ${1 2 3:L:ts*:Ua b c:M*} != "a*b*c"
.  error
.endif
# In this expression, the ':ts*' is indirect, therefore the changed separator
# only applies to the modifiers from the indirect text.  It does not affect
# the ':M' since that is not part of the text from the indirect modifier.
#
# Implementation detail: when ApplyModifiersIndirect calls ApplyModifiers
# (which creates a new ModChain containing a fresh separator),
# the outer separator character is not passed by reference to the inner
# evaluation, therefore the scope of the inner separator ends after applying
# the modifier ':ts*'.
.if ${1 2 3:L:${:Uts*}:Ua b c:M*} != "a b c"
.  error
.endif

# A direct modifier ':U' turns the expression from undefined to defined.
# An indirect modifier ':U' has the same effect, unlike the separator from
# ':ts*' or the single-word marker from ':tW'.
#
# This is because when ApplyModifiersIndirect calls ApplyModifiers, it passes
# the definedness of the outer expression by reference.  If that weren't the
# case, the first condition below would result in a parse error because its
# left-hand side would be undefined.
.if ${UNDEF:${:UUindirect-fallback}} != "indirect-fallback"
.  error
.endif
.if ${UNDEF:${:UUindirect-fallback}:Uouter-fallback} != "outer-fallback"
.  error
.endif

all:
