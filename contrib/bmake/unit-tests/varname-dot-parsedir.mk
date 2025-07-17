# $NetBSD: varname-dot-parsedir.mk,v 1.8 2023/06/21 07:30:50 rillig Exp $
#
# Tests for the special .PARSEDIR variable, which contains the directory part
# of the file that is currently parsed.
#
# See also
#	varname-dot-includedfromdir.mk
#	varname-dot-includedfromfile.mk
#	varname-dot-parsefile.mk
#
# History
#	.PARSEDIR and .PARSEFILE were added on 1999-08-09.

# The .PARSEDIR may be absolute or relative, therefore there is not much that
# can be tested here.
.if !${.PARSEDIR:tA:M*/unit-tests}
.  error
.endif

# During parsing, it is possible to undefine .PARSEDIR.
# Not that anyone would ever want to do this, but there's code in parse.c,
# function PrintLocation, that explicitly handles this situation.
.if !defined(.PARSEDIR)
.  error
.endif
.undef .PARSEDIR
.if defined(.PARSEDIR)
.  error
.endif

# The variable .PARSEDIR is indirectly used by the .info directive,
# via PrintLocation.
#
# The .rawout file contains the full path to the current directory.
# In the .out file, it is filtered out.
# expect+1: At this point, .PARSEDIR is undefined.
.info At this point, .PARSEDIR is undefined.

# There is absolutely no point in faking the location of the file that is
# being parsed.  Technically, it's possible though, but only if the file
# being parsed is a relative pathname.  See PrintLocation for details.
.PARSEDIR=	/fake-absolute-path
.info The location can be faked in some cases.

# After including another file, .PARSEDIR is reset.
.include "/dev/null"
# expect+1: The location is no longer fake.
.info The location is no longer fake.

all:
	@echo At run time, .PARSEDIR is ${.PARSEDIR:Uundefined}.
