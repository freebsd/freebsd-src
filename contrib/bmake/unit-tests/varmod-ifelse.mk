# $NetBSD: varmod-ifelse.mk,v 1.23 2023/07/01 09:06:34 rillig Exp $
#
# Tests for the ${cond:?then:else} variable modifier, which evaluates either
# the then-expression or the else-expression, depending on the condition.
#
# The modifier was added on 1998-04-01.
#
# Until 2015-10-11, the modifier always evaluated both the "then" and the
# "else" expressions.

# TODO: Implementation

# The variable name of the expression is expanded and then taken as the
# condition.  In the below example it becomes:
#
#	variable expression == "literal"
#
# This confuses the parser, which expects an operator instead of the bare
# word "expression".  If the name were expanded lazily, everything would be
# fine since the condition would be:
#
#	${:Uvariable expression} == "literal"
#
# Evaluating the variable name lazily would require additional code in
# Var_Parse and ParseVarname, it would be more useful and predictable
# though.
# expect+1: Malformed conditional (${${:Uvariable expression} == "literal":?bad:bad})
.if ${${:Uvariable expression} == "literal":?bad:bad}
.  error
.else
.  error
.endif

# In a variable assignment, undefined variables are not an error.
# Because of the early expansion, the whole condition evaluates to
# ' == ""' though, which cannot be parsed because the left-hand side looks
# empty.
COND:=	${${UNDEF} == "":?bad-assign:bad-assign}

# In a condition, undefined variables generate a "Malformed conditional"
# error.  That error message is wrong though.  In lint mode, the correct
# "Undefined variable" error message is generated.
# The difference to the ':=' variable assignment is the additional
# "Malformed conditional" error message.
# expect+1: Malformed conditional (${${UNDEF} == "":?bad-cond:bad-cond})
.if ${${UNDEF} == "":?bad-cond:bad-cond}
.  error
.else
.  error
.endif

# When the :? is parsed, it is greedy.  The else branch spans all the
# text, up until the closing character '}', even if the text looks like
# another modifier.
.if ${1:?then:else:Q} != "then"
.  error
.endif
.if ${0:?then:else:Q} != "else:Q"
.  error
.endif

# This line generates 2 error messages.  The first comes from evaluating the
# malformed conditional "1 == == 2", which is reported as "Bad conditional
# expression" by ApplyModifier_IfElse.  The variable expression containing that
# conditional therefore returns a parse error from Var_Parse, and this parse
# error propagates to CondEvalExpression, where the "Malformed conditional"
# comes from.
# expect+1: Malformed conditional (${1 == == 2:?yes:no} != "")
.if ${1 == == 2:?yes:no} != ""
.  error
.else
.  error
.endif

# If the "Bad conditional expression" appears in a quoted string literal, the
# error message "Malformed conditional" is not printed, leaving only the "Bad
# conditional expression".
#
# XXX: The left-hand side is enclosed in quotes.  This results in Var_Parse
# being called without VARE_UNDEFERR.  When ApplyModifier_IfElse
# returns AMR_CLEANUP as result, Var_Parse returns varUndefined since the
# value of the variable expression is still undefined.  CondParser_String is
# then supposed to do proper error handling, but since varUndefined is local
# to var.c, it cannot distinguish this return value from an ordinary empty
# string.  The left-hand side of the comparison is therefore just an empty
# string, which is obviously equal to the empty string on the right-hand side.
#
# XXX: The debug log for -dc shows a comparison between 1.0 and 0.0.  The
# condition should be detected as being malformed before any comparison is
# done since there is no well-formed comparison in the condition at all.
.MAKEFLAGS: -dc
.if "${1 == == 2:?yes:no}" != ""
.  error
.else
# expect+1: warning: Oops, the parse error should have been propagated.
.  warning Oops, the parse error should have been propagated.
.endif
.MAKEFLAGS: -d0

# As of 2020-12-10, the variable "VAR" is first expanded, and the result of
# this expansion is then taken as the condition.  To force the variable
# expression in the condition to be evaluated at exactly the right point,
# the '$' of the intended '${VAR}' escapes from the parser in form of the
# expression ${:U\$}.  Because of this escaping, the variable "VAR" and thus
# the condition ends up as "${VAR} == value", just as intended.
#
# This hack does not work for variables from .for loops since these are
# expanded at parse time to their corresponding ${:Uvalue} expressions.
# Making the '$' of the '${VAR}' expression indirect hides this expression
# from the parser of the .for loop body.  See ForLoop_SubstVarLong.
.MAKEFLAGS: -dc
VAR=	value
.if ${ ${:U\$}{VAR} == value:?ok:bad} != "ok"
.  error
.endif
.MAKEFLAGS: -d0

# On 2021-04-19, when building external/bsd/tmux with HAVE_LLVM=yes and
# HAVE_GCC=no, the following conditional generated this error message:
#
#	make: Bad conditional expression 'string == "literal" && no >= 10'
#	    in 'string == "literal" && no >= 10?yes:no'
#
# Despite the error message (which was not clearly marked with "error:"),
# the build continued, for historical reasons, see main_Exit.
#
# The tricky detail here is that the condition that looks so obvious in the
# form written in the makefile becomes tricky when it is actually evaluated.
# This is because the condition is written in the place of the variable name
# of the expression, and in an expression, the variable name is always
# expanded first, before even looking at the modifiers.  This happens for the
# modifier ':?' as well, so when CondEvalExpression gets to see the
# expression, it already looks like this:
#
#	string == "literal" && no >= 10
#
# When parsing such an expression, the parser used to be strict.  It first
# evaluated the left-hand side of the operator '&&' and then started parsing
# the right-hand side 'no >= 10'.  The word 'no' is obviously a string
# literal, not enclosed in quotes, which is OK, even on the left-hand side of
# the comparison operator, but only because this is a condition in the
# modifier ':?'.  In an ordinary directive '.if', this would be a parse error.
# For strings, only the comparison operators '==' and '!=' are defined,
# therefore parsing stopped at the '>', producing the 'Bad conditional
# expression'.
#
# Ideally, the conditional expression would not be expanded before parsing
# it.  This would allow to write the conditions exactly as seen below.  That
# change has a high chance of breaking _some_ existing code and would need
# to be thoroughly tested.
#
# Since cond.c 1.262 from 2021-04-20, make reports a more specific error
# message in situations like these, pointing directly to the specific problem
# instead of just saying that the whole condition is bad.
STRING=		string
NUMBER=		no		# not really a number
# expect+1: no.
.info ${${STRING} == "literal" && ${NUMBER} >= 10:?yes:no}.
# expect+3: Comparison with '>=' requires both operands 'no' and '10' to be numeric
# expect: make: Bad conditional expression 'string == "literal" || no >= 10' in 'string == "literal" || no >= 10?yes:no'
# expect+1: .
.info ${${STRING} == "literal" || ${NUMBER} >= 10:?yes:no}.

# The following situation occasionally occurs with MKINET6 or similar
# variables.
NUMBER=		# empty, not really a number either
# expect: make: Bad conditional expression 'string == "literal" &&  >= 10' in 'string == "literal" &&  >= 10?yes:no'
# expect+1: .
.info ${${STRING} == "literal" && ${NUMBER} >= 10:?yes:no}.
# expect: make: Bad conditional expression 'string == "literal" ||  >= 10' in 'string == "literal" ||  >= 10?yes:no'
# expect+1: .
.info ${${STRING} == "literal" || ${NUMBER} >= 10:?yes:no}.

# CondParser_LeafToken handles [0-9-+] specially, treating them as a number.
PLUS=		+
ASTERISK=	*
EMPTY=		# empty
# "true" since "+" is not the empty string.
# expect+1: <true>
.info <${${PLUS}		:?true:false}>
# "false" since the variable named "*" is not defined.
# expect+1: <false>
.info <${${ASTERISK}	:?true:false}>
# syntax error since the condition is completely blank.
# expect+1: <>
.info <${${EMPTY}	:?true:false}>


# Since the condition of the '?:' modifier is expanded before being parsed and
# evaluated, it is common practice to enclose expressions in quotes, to avoid
# producing syntactically invalid conditions such as ' == value'.  This only
# works if the expanded values neither contain quotes nor backslashes.  For
# strings containing quotes or backslashes, the '?:' modifier should not be
# used.
PRIMES=	2 3 5 7 11
.if ${1 2 3 4 5:L:@n@$n:${ ("${PRIMES:M$n}" != "") :?prime:not_prime}@} != \
  "1:not_prime 2:prime 3:prime 4:not_prime 5:prime"
.  error
.endif

# When parsing the modifier ':?', there are 3 possible cases:
#
#	1. The whole expression is only parsed.
#	2. The expression is parsed and the 'then' branch is evaluated.
#	3. The expression is parsed and the 'else' branch is evaluated.
#
# In all of these cases, the expression must be parsed in the same way,
# especially when one of the branches contains unbalanced '{}' braces.
#
# At 2020-01-01, the expressions from the 'then' and 'else' branches were
# parsed differently, depending on whether the branch was taken or not.  When
# the branch was taken, the parser recognized that in the modifier ':S,}},,',
# the '}}' were ordinary characters.  When the branch was not taken, the
# parser only counted balanced '{' and '}', ignoring any escaping or other
# changes in the interpretation.
#
# In var.c 1.285 from 2020-07-20, the parsing of the expressions changed so
# that in both cases the expression is parsed in the same way, taking the
# unbalanced braces in the ':S' modifiers into account.  This change was not
# on purpose, the commit message mentioned 'has the same effect', which was a
# wrong assumption.
#
# In var.c 1.323 from 2020-07-26, the unintended fix from var.c 1.285 was
# reverted, still not knowing about the difference between regular parsing and
# balanced-mode parsing.
#
# In var.c 1.1028 from 2022-08-08, there was another attempt at fixing this
# inconsistency in parsing, but since that broke parsing of the modifier ':@',
# it was reverted in var.c 1.1029 from 2022-08-23.
#
# In var.c 1.1047 from 2023-02-18, the inconsistency in parsing was finally
# fixed.  The modifier ':@' now parses the body in balanced mode, while
# everywhere else the modifier parts have their subexpressions parsed in the
# same way, no matter whether they are evaluated or not.
#
# The modifiers ':@' and ':?' are similar in that they conceptually contain
# text to be evaluated later or conditionally, still they parse that text
# differently.  The crucial difference is that the body of the modifier ':@'
# is always parsed using balanced mode.  The modifier ':?', on the other hand,
# must parse both of its branches in the same way, no matter whether they are
# evaluated or not.  Since balanced mode and standard mode are incompatible,
# it's impossible to use balanced mode in the modifier ':?'.
.MAKEFLAGS: -dc
.if 0 && ${1:?${:Uthen0:S,}},,}:${:Uelse0:S,}},,}} != "not evaluated"
# At 2020-01-07, the expression evaluated to 'then0,,}}', even though it was
# irrelevant as the '0' had already been evaluated to 'false'.
.  error
.endif
.if 1 && ${0:?${:Uthen1:S,}},,}:${:Uelse1:S,}},,}} != "else1"
.  error
.endif
.if 2 && ${1:?${:Uthen2:S,}},,}:${:Uelse2:S,}},,}} != "then2"
# At 2020-01-07, the whole expression evaluated to 'then2,,}}' instead of the
# expected 'then2'.  The 'then' branch of the ':?' modifier was parsed
# normally, parsing and evaluating the ':S' modifier, thereby treating the
# '}}' as ordinary characters and resulting in 'then2'.  The 'else' branch was
# parsed in balanced mode, ignoring that the inner '}}' were ordinary
# characters.  The '}}' were thus interpreted as the end of the 'else' branch
# and the whole expression.  This left the trailing ',,}}', which together
# with the 'then2' formed the result 'then2,,}}'.
.  error
.endif


# Since the condition is taken from the variable name of the expression, not
# from its value, it is evaluated early.  It is possible though to construct
# conditions that are evaluated lazily, at exactly the right point.  There is
# no way to escape a '$' directly in the variable name, but there are
# alternative ways to bring a '$' into the condition.
#
#	In an indirect condition using the ':U' modifier, each '$', ':' and
#	'}' must be escaped as '\$', '\:' and '\}', respectively, but '{' must
#	not be escaped.
#
#	In an indirect condition using a separate variable, each '$' must be
#	escaped as '$$'.
#
# These two forms allow the variables to contain arbitrary characters, as the
# condition parser does not see them.
DELAYED=	two
# expect+1: no
.info ${ ${:U \${DELAYED\} == "one"}:?yes:no}
# expect+1: yes
.info ${ ${:U \${DELAYED\} == "two"}:?yes:no}
INDIRECT_COND1=	$${DELAYED} == "one"
# expect+1: no
.info ${ ${INDIRECT_COND1}:?yes:no}
INDIRECT_COND2=	$${DELAYED} == "two"
# expect+1: yes
.info ${ ${INDIRECT_COND2}:?yes:no}


.MAKEFLAGS: -d0
