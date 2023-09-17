# $NetBSD: varname-dot-path.mk,v 1.3 2020/10/02 18:46:54 rillig Exp $
#
# Tests for the special .PATH variable, which TODO: describe the purpose.

_!=	mkdir -p varname-dot-path.d

# By default, .PATH consists of "." and .CURDIR.
# XXX: Why both? Shouldn't they have the same effect?
.if ${.PATH} != ". ${.CURDIR}"
.  error ${.PATH}
.endif

# The special target .PATH adds a directory to the path.
.PATH: /
.if ${.PATH} != ". ${.CURDIR} /"
.  error ${.PATH}
.endif

# Only existing directories are added to the path, the others are ignored.
.PATH: /nonexistent
.if ${.PATH} != ". ${.CURDIR} /"
.  error ${.PATH}
.endif

# Only directories are added to the path, not regular files.
.PATH: ${.PARSEDIR}/${.PARSEFILE}
.if ${.PATH} != ". ${.CURDIR} /"
.  error ${.PATH}
.endif

# Relative directories can be added as well.
# Each directory is only added once to the path.
.PATH: varname-dot-path.d /
.if ${.PATH} != ". ${.CURDIR} / varname-dot-path.d"
.  error ${.PATH}
.endif

# The pathnames are not normalized before being added to the path.
.PATH: ./.
.if ${.PATH} != ". ${.CURDIR} / varname-dot-path.d ./."
.  error ${.PATH}
.endif

# The two default entries can be placed at the back of the path,
# by adding the special entry ".DOTLAST" somewhere in the path.
# The entry .DOTLAST, if any, is listed in the path, always at the
# very beginning, to make this magic less surprising.
.PATH: .DOTLAST
.if ${.PATH} != ".DOTLAST / varname-dot-path.d ./. . ${.CURDIR}"
.  error ${.PATH}
.endif

_!=	rmdir varname-dot-path.d

all:
	@:;
