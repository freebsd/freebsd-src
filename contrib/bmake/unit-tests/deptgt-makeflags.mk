# $NetBSD: deptgt-makeflags.mk,v 1.9 2023/11/19 22:32:44 rillig Exp $
#
# Tests for the special target .MAKEFLAGS in dependency declarations,
# which adds command line options later, at parse time.
#
# In these unit tests, it is often used to temporarily toggle the debug log
# during parsing.

# The -D option sets a variable in the "Global" scope and thus can be
# undefined later.
.MAKEFLAGS: -D VAR
.if ${VAR} != 1
.  error
.endif

# Variables that are set via the -D command line option are normal global
# variables and can thus be undefined later.
.undef VAR
.if defined(VAR)
.  error
.endif

# The -D command line option can define a variable again, after it has been
# undefined.
.MAKEFLAGS: -D VAR
.if ${VAR} != 1
.  error
.endif

# The "dependency" for .MAKEFLAGS is split into words, interpreting the usual
# quotes and escape sequences from the backslash.
.MAKEFLAGS: VAR="value"' with'\ spaces
.if ${VAR} != "value with spaces"
.  error
.endif

# Variables set on the command line as VAR=value are placed in the
# "Command" scope and thus cannot be undefined.
.undef VAR
.if ${VAR} != "value with spaces"
.  error
.endif

# When parsing this line, each '$$' becomes '$', resulting in '$$$$'.
# This is assigned to the variable DOLLAR.
# In the condition, that variable is expanded, and at that point, each '$$'
# becomes '$' again, the final expression is thus '$$'.
.MAKEFLAGS: -dcv
.MAKEFLAGS: DOLLAR=$$$$$$$$
.if ${DOLLAR} != "\$\$"
.endif
.MAKEFLAGS: -d0

# An empty command line is skipped.
.MAKEFLAGS: # none

# Escape sequences like \n are interpreted.
# The following line looks as if it assigned a newline to nl, but it doesn't.
# Instead, the \n ends up as a line that is then interpreted as a variable
# assignment.  At that point, the line is simply "nl=\n", and the \n is
# skipped since it is whitespace (see Parse_IsVar).
.MAKEFLAGS: nl="\n"
.if ${nl} != ""
.  error
.endif

# Next try at defining another newline variable.  Since whitespace around the
# variable value is trimmed, two empty expressions ${:U} surround the
# literal newline now.  This prevents the newline from being skipped during
# parsing.  The ':=' assignment operator expands the empty
# expressions, leaving only the newline as the variable value.
#
# This is one of the very few ways (maybe even the only one) to inject literal
# newlines into a line that is being parsed.  This may confuse the parser.
# For example, in cond.c the parser only expects horizontal whitespace (' '
# and '\t'), but no newlines.
#.MAKEFLAGS: -dcpv
.MAKEFLAGS: nl:="$${:U}\n$${:U}"
.if ${nl} != ${.newline}
.  error
.endif
#.MAKEFLAGS: -d0

# Now do the same for the other escape sequences; see Substring_Words.
.MAKEFLAGS: CHAR_BS:="$${:U}\b$${:U}"
.MAKEFLAGS: CHAR_FF:="$${:U}\f$${:U}"
.MAKEFLAGS: CHAR_NL:="$${:U}\n$${:U}"
.MAKEFLAGS: CHAR_CR:="$${:U}\r$${:U}"
.MAKEFLAGS: CHAR_TAB:="$${:U}\t$${:U}"

# Note: backspace is not whitespace, it is a control character.
.if ${CHAR_BS:C,^[[:cntrl:]]$,found,W} != "found"
.  error
.endif
.if ${CHAR_FF:C,^[[:space:]]$,found,W} != "found"
.  error
.endif
.if ${CHAR_NL:C,^[[:space:]]$,found,W} != "found"
.  error
.endif
.if ${CHAR_CR:C,^[[:space:]]$,found,W} != "found"
.  error
.endif
.if ${CHAR_TAB:C,^[[:space:]]$,found,W} != "found"
.  error
.endif


# Unbalanced quotes produce an error message.  If they occur anywhere in the
# command line, the whole command line is skipped.
.MAKEFLAGS: VAR=previous
.MAKEFLAGS: VAR=initial UNBALANCED='
.if ${VAR} != "previous"
.  error
.endif

all:
	@:;
