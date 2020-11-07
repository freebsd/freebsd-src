# $NetBSD: directive-export.mk,v 1.3 2020/10/29 17:27:12 rillig Exp $
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

all:
	@:;
