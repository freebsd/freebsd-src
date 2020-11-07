# $NetBSD: deptgt-makeflags.mk,v 1.4 2020/10/23 14:48:49 rillig Exp $
#
# Tests for the special target .MAKEFLAGS in dependency declarations,
# which adds command line options later, at parse time.

# The -D option sets a variable in the "Global" scope and thus can be
# undefined later.
.MAKEFLAGS: -D VAR

.if ${VAR} != 1
.  error
.endif

.undef VAR

.if defined(VAR)
.  error
.endif

.MAKEFLAGS: -D VAR

.if ${VAR} != 1
.  error
.endif

.MAKEFLAGS: VAR="value"' with'\ spaces

.if ${VAR} != "value with spaces"
.  error
.endif

# Variables set on the command line as VAR=value are placed in the
# "Command" scope and thus cannot be undefined.
.undef VAR

.if ${VAR} != "value with spaces"
.  error
.endif

# When parsing this line, each '$$' becomes '$', resulting in '$$$$'.
# This is assigned to the variable DOLLAR.
# In the condition, that variable is expanded, and at that point, each '$$'
# becomes '$' again, the final expression is thus '$$'.
.MAKEFLAGS: -dcv
.MAKEFLAGS: DOLLAR=$$$$$$$$
.if ${DOLLAR} != "\$\$"
.endif
.MAKEFLAGS: -d0

all:
	@:;
