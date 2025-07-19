# $NetBSD: directive.mk,v 1.10 2025/06/28 22:39:28 rillig Exp $
#
# Tests for the preprocessing directives, such as .if or .info.

# TODO: Implementation

# Unknown directives are correctly named in the error messages,
# even if they are indented.
# expect+1: Unknown directive "indented"
.indented none
# expect+1: Unknown directive "indented"
.  indented 2 spaces
# expect+1: Unknown directive "indented"
.	indented tab

# Directives must be written directly, not indirectly via
# expressions.
# expect+1: Unknown directive ""
.${:Uinfo} directives cannot be indirect

# There is no directive called '.target', therefore this is parsed as a
# dependency declaration with 2 targets and 1 source.
.target target: source

# The following lines demonstrate how the parser tells an .info message apart
# from a variable assignment to ".info", which syntactically is very similar.
.MAKEFLAGS: -dv
.info:=		value		# This is a variable assignment.
.info?=		value		# This is a variable assignment as well.
# expect+1: :=	value
.info :=	value		# The space after the '.info' makes this
				# a directive.
.MAKEFLAGS: -d0

# This is a dependency since directives must be given directly.
# Not even the space after the '.info' can change anything about this.
.${:Uinfo} : source

# expect+1: Invalid line "target-without-colon"
target-without-colon

# expect+1: Invalid line "target-without-colon another-target"
target-without-colon another-target
