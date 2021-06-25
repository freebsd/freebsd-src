# $NetBSD: directive-export.mk,v 1.8 2021/02/16 19:01:18 rillig Exp $
#
# Tests for the .export directive.
#
# See also:
#	directive-misspellings.mk

# TODO: Implementation

INDIRECT=	indirect
VAR=		value $$ ${INDIRECT}

# Before 2020-12-13, this unusual expression invoked undefined behavior since
# it accessed out-of-bounds memory via Var_Export -> ExportVar -> MayExport.
.export ${:U }

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

# No syntactical argument means to export all variables.
.export

# An empty argument means no additional variables to export.
.export ${:U}


# Trigger the "This isn't going to end well" in ExportVarEnv.
EMPTY_SHELL=	${:sh}
.export EMPTY_SHELL	# only marked for export at this point
_!=		:;:	# Force the variable to be actually exported.


all:
