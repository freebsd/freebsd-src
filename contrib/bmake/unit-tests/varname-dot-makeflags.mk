# $NetBSD: varname-dot-makeflags.mk,v 1.7 2023/02/25 19:24:07 rillig Exp $
#
# Tests for the special .MAKEFLAGS variable, which collects almost all
# command line arguments and passes them on to any child processes via
# the environment variable MAKEFLAGS (without leading '.').
#
# See also:
#	varname-dot-makeoverrides.mk

.info MAKEFLAGS=<${MAKEFLAGS:Uundefined}>
.info .MAKEFLAGS=<${.MAKEFLAGS}>
.info .MAKEOVERRIDES=<${.MAKEOVERRIDES:Uundefined}>

# Append an option with argument, a plain option and a variable assignment.
.MAKEFLAGS: -DVARNAME -r VAR=value

# expect+1: MAKEFLAGS=<undefined>
.info MAKEFLAGS=<${MAKEFLAGS:Uundefined}>
# expect+1: .MAKEFLAGS=< -r -k -D VARNAME -r>
.info .MAKEFLAGS=<${.MAKEFLAGS}>
# expect+1: .MAKEOVERRIDES=< VAR>
.info .MAKEOVERRIDES=<${.MAKEOVERRIDES}>

# The environment variable 'MAKEFLAGS' is not available to child processes
# when parsing the makefiles.  This is different from exported variables,
# which are already available during parse time.
.if ${:!echo "\${MAKEFLAGS-undef}"!} != "undef"
.  error
.endif

# After parsing, the environment variable 'MAKEFLAGS' is set based on the
# special variables '.MAKEFLAGS' and '.MAKEOVERRIDES'.
runtime:
	@echo '$@: MAKEFLAGS=<'${MAKEFLAGS:Q}'>'
	@echo '$@: .MAKEFLAGS=<'${.MAKEFLAGS:Q}'>'
	@echo '$@: .MAKEOVERRIDES=<'${.MAKEOVERRIDES:Q}'>'
