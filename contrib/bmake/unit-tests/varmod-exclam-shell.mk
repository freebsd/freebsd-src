# $NetBSD: varmod-exclam-shell.mk,v 1.3 2020/10/24 08:46:08 rillig Exp $
#
# Tests for the :!cmd! variable modifier.

.if ${:!echo hello | tr 'l' 'l'!} != "hello"
.  warning unexpected
.endif

# The output is truncated at the first null byte.
# Cmd_Exec returns only a string pointer without length information.
.if ${:!echo hello | tr 'l' '\0'!} != "he"
.  warning unexpected
.endif

.if ${:!echo!} != ""
.  warning A newline at the end of the output must be stripped.
.endif

.if ${:!echo;echo!} != " "
.  warning Only a single newline at the end of the output is stripped.
.endif

.if ${:!echo;echo;echo;echo!} != "   "
.  warning Other newlines in the output are converted to spaces.
.endif

all:
	@:;
