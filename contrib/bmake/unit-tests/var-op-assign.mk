# $NetBSD: var-op-assign.mk,v 1.10 2023/08/19 10:52:14 rillig Exp $
#
# Tests for the = variable assignment operator, which overwrites an existing
# variable or creates it.

# This is a simple variable assignment.
# To the left of the assignment operator '=' there is the variable name,
# and to the right is the variable value.  The variable value is stored as-is,
# it is not expanded in any way.
#
VAR=	value

# This condition demonstrates that whitespace around the assignment operator
# is discarded.  Otherwise the value would start with a single tab.
#
.if ${VAR} != "value"
.  error
.endif

# Whitespace to the left of the assignment operator is ignored as well.
# The variable value can contain arbitrary characters.
#
# The '#' needs to be escaped with a backslash, this happens in a very
# early stage of parsing and applies to all line types, except for the
# commands, which are indented with a tab.
#
# The '$' needs to be escaped with another '$', otherwise it would refer to
# another variable.
#
VAR=	new value and \# some $$ special characters	# comment

# When a string literal appears in a condition, the escaping rules are
# different.  Run make with the -dc option to see the details.
.if ${VAR} != "new value and \# some \$ special characters"
.  error ${VAR}
.endif

# The variable value may contain references to other variables.
# In this example, the reference is to the variable with the empty name,
# which is never defined.
#
# This alone would not produce any side-effects, therefore the variable has
# a :!...! modifier that executes a shell command.  The :!...! modifier turns
# an undefined expression into a defined one, see ApplyModifier_ShellCommand,
# the call to Expr_Define.
#
# Since the right-hand side of a '=' assignment is not expanded at the time
# when the variable is defined, the first command is not run at all.
VAR=	${:! echo 'not yet evaluated' 1>&2 !}
VAR=	${:! echo 'this will be evaluated later' 1>&2 !}

# Now force the variable to be evaluated.
# This outputs the line to stderr.
.if ${VAR}
.endif

# In a variable assignment, the variable name must consist of a single word.
# The following line therefore generates a parse error.
# expect+1: Invalid line 'VARIABLE NAME=	variable value'
VARIABLE NAME=	variable value

# But if the whitespace appears inside parentheses or braces, everything is
# fine.
#
# XXX: This was not an intentional decision, as variable names typically
# neither contain parentheses nor braces.  This is only a side-effect from
# the implementation of the parser, which cheats when parsing a variable
# name.  It only counts parentheses and braces instead of properly parsing
# nested variable expressions such as VAR.${param}.
#
VAR(spaces in parentheses)=	()
VAR{spaces in braces}=		{}

# Be careful and use indirect variable names here, to prevent accidentally
# accepting the test in case the parser just uses "VAR" as the variable name,
# ignoring all the rest.
#
VARNAME_PAREN=	VAR(spaces in parentheses)
VARNAME_BRACES=	VAR{spaces in braces}

.if ${${VARNAME_PAREN}} != "()"
.  error
.endif

.if ${${VARNAME_BRACES}} != "{}"
.  error
.endif

# In safe mode, parsing would stop immediately after the "VARIABLE NAME="
# line, since any commands run after that are probably working with
# unexpected variable values.
#
# Therefore, just output an info message.
# expect+1: Parsing still continues until here.
.info Parsing still continues until here.

all:
	@:;
