# $NetBSD: directive-export.mk,v 1.4 2020/11/03 17:17:31 rillig Exp $
#
# Tests for the .export directive.

# TODO: Implementation

INDIRECT=	indirect
VAR=		value $$ ${INDIRECT}

# A variable is exported using the .export directive.
# During that, its value is expanded, just like almost everywhere else.
.export VAR
.if ${:!env | grep '^VAR'!} != "VAR=value \$ indirect"
.  error
.endif

# Undefining a variable that has been exported implicitly removes it from
# the environment of all child processes.
.undef VAR
.if ${:!env | grep '^VAR' || true!} != ""
.  error
.endif

# Tests for parsing the .export directive.
.expor				# misspelled
.export				# oops: missing argument
.export VARNAME
.exporting works		# oops: misspelled

all:
	@:;
