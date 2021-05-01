# $NetBSD: directive.mk,v 1.4 2020/11/15 11:57:00 rillig Exp $
#
# Tests for the preprocessing directives, such as .if or .info.

# TODO: Implementation

# Unknown directives are correctly named in the error messages,
# even if they are indented.
.indented none
.  indented 2 spaces
.	indented tab

# Directives must be written directly, not indirectly via variable
# expressions.
.${:Uinfo} directives cannot be indirect

# There is no directive called '.target', therefore this is parsed as a
# dependency declaration with 2 targets and 1 source.
.target target: source

# This looks ambiguous.  It could be either an .info message or a variable
# assignment.  It is a variable assignment.
.MAKEFLAGS: -dv
.info:=		value
.info?=		value		# This is a variable assignment as well.
.info :=	value		# The space after the '.info' makes this
				# a directive.
.MAKEFLAGS: -d0

# This is a dependency since directives must be given directly.
# Not even the space after the '.info' can change anything about this.
.${:Uinfo} : source

all:
	@:;
