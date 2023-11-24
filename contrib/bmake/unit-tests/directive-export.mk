# $NetBSD: directive-export.mk,v 1.9 2023/08/20 20:48:32 rillig Exp $
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


# If the '.export' directive exports a variable whose value contains a '$',
# the actual export action is deferred until a subprocess is started, assuming
# that only subprocesses access the environment variables.  The ':localtime'
# modifier depends on the 'TZ' environment variable, without any subprocess.
export TZ=${UTC}
# expect+1: 00:00:00
.info ${%T:L:localtime=86400}
INDIRECT_TZ=	${:UAmerica/Los_Angeles}
TZ=		${INDIRECT_TZ}
.export TZ
# expect+1: 00:00:00
.info ${%T:L:localtime=86400}
_!=	echo 'force exporting the environment variables'
# expect+1: 16:00:00
.info ${%T:L:localtime=86400}


all:
