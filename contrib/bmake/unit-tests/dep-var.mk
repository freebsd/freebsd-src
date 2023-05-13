# $NetBSD: dep-var.mk,v 1.8 2023/05/10 15:53:32 rillig Exp $
#
# Tests for variable references in dependency declarations.
#
# Uh oh, this feels so strange that probably nobody uses it. But it seems to
# be the only way to reach the lower half of SuffExpandChildren.

.MAKEFLAGS: -dv

# expect: Var_Parse: ${UNDEF1} (eval-defined)
# Even though undefined expressions should lead to errors, no error message is
# generated for this line.  The variable expression ${UNDEF1} simply expands
# to an empty string.
all: ${UNDEF1}

# Using a double dollar in order to circumvent immediate variable expansion
# feels like unintended behavior.  At least the manual page says nothing at
# all about defined or undefined variables in dependency lines.
#
# At the point where the expression ${DEF2} is expanded, the variable DEF2
# is defined, so everything's fine.
all: $${DEF2} a-$${DEF2}-b

# This variable is not defined at all.
# XXX: The -dv log says later when expanding the sources of 'all':
#	Var_Parse: ${UNDEF3} (eval-defined)
# but no error message is generated for this line, just like for UNDEF1.
# The variable expression ${UNDEF3} simply expands to an empty string.
all: $${UNDEF3}

# Try out how many levels of indirection are really expanded in dependency
# lines.
#
# The first level of indirection is the $$ in the dependency line.
# When the dependency line is parsed, it is resolved to the string
# "${INDIRECT_1}".  At this point, the dollar is just an ordinary character,
# waiting to be expanded at some later point.
#
# Later, in SuffExpandChildren, that expression is expanded again by calling
# Var_Parse, and this time, the result is the string "1-2-${INDIRECT_2}-2-1".
#
# This string is not expanded anymore by Var_Parse.  But there is another
# effect.  Now DirExpandCurly comes into play and expands the curly braces
# in this filename pattern, resulting in the string "1-2-$INDIRECT_2-2-1".
# As of 2020-09-03, the test dir.mk contains further details on this topic.
#
# Finally, this string is assigned to the local ${.TARGET} variable.  This
# variable is expanded when the shell command is generated.  At that point,
# the $I is expanded.  Since the variable I is not defined, it expands to
# the empty string.  This way, the final output is the string
# "1-2-NDIRECT_2-2-1", which differs from the actual name of the target.
# For exactly this reason, it is not recommended to use dollar signs in
# target names.
#
# The number of actual expansions is way more than one might expect,
# therefore this feature is probably not widely used.
#
all: 1-$${INDIRECT_1}-1
INDIRECT_1=	2-$${INDIRECT_2}-2
INDIRECT_2=	3-$${INDIRECT_3}-3
INDIRECT_3=	indirect

UNDEF1=	undef1
DEF2=	def2

# Cover the code in SuffExpandChildren that deals with malformed variable
# expressions.
#
# This seems to be an edge case that never happens in practice, and it would
# probably be appropriate to just error out in such a case.
#
# To trigger this piece of code, the variable name must contain "$)" or "$:"
# or "$)" or "$$".  Using "$:" does not work since the dependency line is
# fully expanded before parsing, therefore any ':' in a target or source name
# would be interpreted as a dependency operator instead.
all: $$$$)

# The $$INDIRECT in the following line is treated like the dependency of the
# "all" target, that is, the "$$I" is first expanded to "$I", and in a second
# round of expansion, the "$I" expands to nothing since the variable "I" is
# undefined.
#
# Since 2020-09-13, this generates a parse error in lint mode (-dL), but not
# in normal mode since ParseDependency does not handle any errors after
# calling Var_Parse.
# expect: Var_Parse: ${:U\$)}: (eval-defined)
# expect: Var_Parse: $INDIRECT_2-2-1 $): (parse-only)
# expect: Var_Parse: $): (parse-only)
undef1 def2 a-def2-b 1-2-$$INDIRECT_2-2-1 ${:U\$)}:
	@echo ${.TARGET:Q}

.MAKEFLAGS: -d0

# XXX: The exit status is still 0, even though Parse_Error is called with
# PARSE_FATAL in SuffExpandChildren.  The exit status is only affected by
# parse errors when they occur in the parsing phase, see Parse_File.
