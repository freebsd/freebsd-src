# $NetBSD: varname-dot-suffixes.mk,v 1.1 2021/12/12 22:16:48 rillig Exp $
#
# Tests for the special "variable" .SUFFIXES, which lists the suffixes that
# have been registered for use in suffix transformation rules.  Suffixes are
# listed even if there is no actual transformation rule that uses them.
#
# The name '.SUFFIXES' does not refer to a real variable, instead it can be
# used as a starting "variable name" for expressions like ${.SUFFIXES} or
# ${.SUFFIXES:M*o}.

# In the beginning, there are no suffix rules, the expression is thus empty.
.if ${.SUFFIXES} != ""
.endif

# There is no actual variable named '.SUFFIXES', it is all made up.
.if defined(.SUFFIXES)
.  error
.endif

# The suffixes list is still empty, and so is the "variable" '.SUFFIXES'.
.if !empty(.SUFFIXES)
.  error
.endif

.SUFFIXES: .c .o .1		.err

# The suffixes are listed in declaration order.
.if ${.SUFFIXES} != ".c .o .1 .err"
.  error
.endif

# There is still no actual variable named '.SUFFIXES', it is all made up.
.if defined(.SUFFIXES)
.  error
.endif

# Now the suffixes list is not empty anymore.  It may seem strange that there
# is no variable named '.SUFFIXES' but evaluating '${.SUFFIXES}' nevertheless
# returns something.  For all practical use cases, it's good enough though.
.if empty(.SUFFIXES)
.  error
.endif

.SUFFIXES: .tar.gz

# Changes to the suffixes list are reflected immediately.
.if ${.SUFFIXES} != ".c .o .1 .err .tar.gz"
.  error
.endif

# Deleting .SUFFIXES has no effect since there is no actual variable of that
# name.
.MAKEFLAGS: -dv
# expect: Global:delete .SUFFIXES (not found)
.undef .SUFFIXES
.MAKEFLAGS: -d0
.if ${.SUFFIXES} != ".c .o .1 .err .tar.gz"
.  error
.endif

# The list of suffixes can only be modified using dependency declarations, any
# attempt at setting the variable named '.SUFFIXES' is rejected.
.MAKEFLAGS: -dv
# expect: Global: .SUFFIXES = set ignored (read-only)
.SUFFIXES=	set
# expect: Global: .SUFFIXES = append ignored (read-only)
.SUFFIXES+=	append
# expect: Global: .SUFFIXES = assign ignored (read-only)
_:=		${.SUFFIXES::=assign}
# expect: Command: .SUFFIXES = preserve ignored (read-only)
_:=		${preserve:L:_=.SUFFIXES}
.MAKEFLAGS: -d0

# Using the name '.SUFFIXES' in a .for loop looks strange because these
# variable names are typically in singular form, and .for loops do not use
# real variables either, they are made up as well, see directive-for.mk.  The
# replacement mechanism for the iteration variables takes precedence.
.for .SUFFIXES in .c .o
.  if ${.SUFFIXES} != ".c" && ${.SUFFIXES} != ".o"
.    error
.  endif
.endfor

# After the .for loop, the expression '${.SUFFIXES}' refers to the list of
# suffixes again.
.if ${.SUFFIXES} != ".c .o .1 .err .tar.gz"
.  error
.endif

# Using the name '.SUFFIXES' in the modifier ':@var@body@' does not create an
# actual variable either.  Like in the .for loop, choosing the name
# '.SUFFIXES' for the iteration variable is unusual.  In ODE Make, the
# convention for these iteration variables is to have dots at both ends, so
# the name would be '.SUFFIXES.', furthermore the name of the iteration
# variable is typically in singular form.
.MAKEFLAGS: -dv
# expect: Command: .SUFFIXES = 1 ignored (read-only)
# expect: Command: .SUFFIXES = 2 ignored (read-only)
.if ${1 2:L:@.SUFFIXES@${.SUFFIXES}@} != ".c .o .1 .err .tar.gz .c .o .1 .err .tar.gz"
.  error
.endif
.MAKEFLAGS: -d0

all:
