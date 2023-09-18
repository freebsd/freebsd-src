# $NetBSD: directive-export-gmake.mk,v 1.7 2023/08/20 20:48:32 rillig Exp $
#
# Tests for the export directive (without leading dot), as in GNU make.

# The "export" directive only affects the environment of the make process
# and its child processes.  It does not affect the global variables or any
# other variables.
VAR=	before
export VAR=exported
.if ${VAR} != "before"
.  error
.endif

# Ensure that the name-value pair is actually exported.
.if ${:!echo "\$VAR"!} != "exported"
.  error
.endif

# This line looks like it would export 2 variables, but it doesn't.
# It only exports VAR and appends everything else as the variable value.
export VAR=exported VAR2=exported-as-well
.if ${:!echo "\$VAR"!} != "exported VAR2=exported-as-well"
.  error ${:!echo "\$VAR"!}
.endif

# Contrary to the usual variable assignments, spaces are significant
# after the '=' sign and are prepended to the value of the environment
# variable.
export VAR=  leading spaces
.if ${:!echo "\$VAR"!} != "  leading spaces"
.  error
.endif

# Contrary to the usual variable assignments, spaces are significant
# before the '=' sign and are appended to the name of the environment
# variable.
#
# Depending on the shell, environment variables with such exotic names
# may be silently discarded.  One such shell is dash, which is the default
# shell on Ubuntu and Debian.
export VAR =trailing space in varname
.if ${:!env | grep trailing || true!} != "VAR =trailing space in varname"
.  if ${:!env | grep trailing || true!} != "" # for dash
.    error
.  endif
.endif

# The right-hand side of the exported variable is expanded exactly once.
TWICE=	expanded twice
ONCE=	expanded once, leaving $${TWICE} as-is
export VAR=${ONCE}
.if ${:!echo "\$VAR"!} != "expanded once, leaving \${TWICE} as-is"
.  error
.endif

# Undefined variables are allowed on the right-hand side, they expand
# to an empty string, as usual.
export VAR=an ${UNDEF} variable
.if ${:!echo "\$VAR"!} != "an  variable"
.  error
.endif


# The body of the .for loop expands to 'export VAR=${:U1}', and the 'export'
# directive is only recognized if the line does not contain a ':', to allow
# 'export' to be a regular target.
.for value in 1
# XXX: The ':' in this line is inside an expression and should thus not be
# interpreted as a dependency operator.
# expect+1: Invalid line 'export VAR=${:U1}'
export VAR=${value}
.endfor


# The 'export' directive expands expressions, but the expressions must not
# contain a ':', due to the overly strict parser.  The indirect expressions
# may contain a ':', though.
#
# As a side effect, this test demonstrates that the 'export' directive exports
# the environment variable immediately, other than the '.export' directive,
# which defers that action if the variable value contains a '$'.
INDIRECT_TZ=	${:UAmerica/Los_Angeles}
export TZ=${INDIRECT_TZ}
# expect+1: 16:00:00
.info ${%T:L:localtime=86400}
