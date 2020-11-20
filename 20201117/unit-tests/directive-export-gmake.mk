# $NetBSD: directive-export-gmake.mk,v 1.3 2020/11/17 20:16:44 rillig Exp $
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

all:
	@:;
