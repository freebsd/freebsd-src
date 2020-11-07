# $NetBSD: varmod.mk,v 1.3 2020/09/13 07:42:20 rillig Exp $
#
# Tests for variable modifiers, such as :Q, :S,from,to or :Ufallback.

DOLLAR1=	$$
DOLLAR2=	${:U\$}

# To get a single '$' sign in the value of a variable expression, it has to
# be written as '$$' in a literal variable value.
#
# See Var_Parse, where it calls Var_Subst.
.if ${DOLLAR1} != "\$"
.  error
.endif

# Another way to get a single '$' sign is to use the :U modifier.  In the
# argument of that modifier, a '$' is escaped using the backslash instead.
#
# See Var_Parse, where it calls Var_Subst.
.if ${DOLLAR2} != "\$"
.  error
.endif

# It is also possible to use the :U modifier directly in the expression.
#
# See Var_Parse, where it calls Var_Subst.
.if ${:U\$} != "\$"
.  error
.endif

# XXX: As of 2020-09-13, it is not possible to use '$$' in a variable name
# to mean a single '$'.  This contradicts the manual page, which says that
# '$' can be escaped as '$$'.
.if ${$$:L} != ""
.  error
.endif

# In lint mode, make prints helpful error messages.
# For compatibility, make does not print these error messages in normal mode.
# Should it?
.MAKEFLAGS: -dL
.if ${$$:L} != ""
.  error
.endif

# A '$' followed by nothing is an error as well.
.if ${:Uword:@word@${word}$@} != "word"
.  error
.endif

all: # nothing
