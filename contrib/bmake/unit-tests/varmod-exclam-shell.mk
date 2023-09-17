# $NetBSD: varmod-exclam-shell.mk,v 1.4 2020/11/03 18:42:33 rillig Exp $
#
# Tests for the :!cmd! variable modifier, which evaluates the modifier
# argument, independent of the value or the name of the original variable.

.if ${:!echo hello | tr 'l' 'l'!} != "hello"
.  error
.endif

# The output is truncated at the first null byte.
# Cmd_Exec returns only a string pointer without length information.
# Truncating the output is not necessarily intended but may also be a side
# effect from the implementation.  Having null bytes in the output of a
# shell command is so unusual that it doesn't matter in practice.
.if ${:!echo hello | tr 'l' '\0'!} != "he"
.  error
.endif

# The newline at the end of the output is stripped.
.if ${:!echo!} != ""
.  error
.endif

# Only the final newline of the output is stripped.  All other newlines are
# converted to spaces.
.if ${:!echo;echo!} != " "
.  error
.endif

# Each newline in the output is converted to a space, except for the newline
# at the end of the output, which is stripped.
.if ${:!echo;echo;echo;echo!} != "   "
.  error
.endif

all:
	@:;
