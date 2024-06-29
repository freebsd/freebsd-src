# $NetBSD: directive-export.mk,v 1.12 2024/06/01 10:06:23 rillig Exp $
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

# Before var.c 1.1117 from 2024-06-01, a plain ".export" without a syntactical
# argument exported all global variables.  This case could be triggered
# unintentionally by writing a line of the form ".export ${VARNAMES}" to a
# makefile, when VARNAMES was an empty list.
# expect+1: warning: .export requires an argument.
.export

# An empty argument means no additional variables to export.
.export ${:U}


# Before a child process is started, whether for the '!=' assignment operator
# or for the ':sh' modifier, all variables that were marked for being exported
# are expanded and then exported.  If expanding such a variable requires
# running a child command, the marked-as-exported variables would need to be
# exported first, ending in an endless loop.  To avoid this endless loop,
# don't export the variables while preparing a child process, see
# ExportVarEnv.
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
