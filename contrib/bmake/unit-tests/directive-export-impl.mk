# $NetBSD: directive-export-impl.mk,v 1.3 2021/04/03 23:08:30 rillig Exp $
#
# Test for the implementation of exporting variables to child processes.
# This involves marking variables for export, actually exporting them,
# or marking them for being re-exported.
#
# See also:
#	Var_Export
#	ExportVar
#	VarExportedMode (global)
#	VarFlags.exported (per variable)
#	VarFlags.reexport (per variable)
#	VarExportMode (per call of Var_Export and ExportVar)

: ${:U:sh}			# side effect: initialize .SHELL

.MAKEFLAGS: -dcpv

# This is a variable that references another variable.  At this point, the
# other variable is still undefined.
UT_VAR=		<${REF}>

# At this point, ExportVar("UT_VAR", VEM_PLAIN) is called.  Since the
# variable value refers to another variable, ExportVar does not actually
# export the variable but only marks it as VarFlags.exported and
# VarFlags.reexport.  After that, ExportVars registers the variable name in
# .MAKE.EXPORTED.  That's all for now.
.export UT_VAR

# The following expression has both flags 'exported' and 'reexport' set.
# These flags do not show up anywhere, not even in the debug log.
: ${UT_VAR:N*}

# At the last moment before actually forking off the child process for the
# :!...! modifier, Cmd_Exec calls Var_ReexportVars to have all relevant
# variables exported.  Since this variable has both of the above-mentioned
# flags set, it is actually exported to the environment.  The variable flags
# are not modified though, since the next time the :!...! modifier is
# evaluated, the referenced variables could have changed, therefore the
# variable will be exported anew for each ':sh' modifier, ':!...!' modifier,
# '!=' variable assignment.
.if ${:!echo "\$UT_VAR"!} != "<>"
.  error
.endif

# The following expression still has 'exported' and 'reexport' set.
# These flags do not show up anywhere though, not even in the debug log.
# These flags means that the variable is still marked as being re-exported
# for each child process.
: ${UT_VAR:N*}

# Now the referenced variable gets defined.  This does not influence anything
# in the process of exporting the variable value, though.
REF=		defined

# Nothing surprising here.  The variable UT_VAR gets exported, and this time,
# REF is defined and gets expanded into the exported environment variable.
.if ${:!echo "\$UT_VAR"!} != "<defined>"
.  error
.endif

all:
.MAKEFLAGS: -d0
