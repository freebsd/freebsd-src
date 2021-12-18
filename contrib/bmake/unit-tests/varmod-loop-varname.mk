# $NetBSD: varmod-loop-varname.mk,v 1.4 2021/12/05 15:01:04 rillig Exp $
#
# Tests for the first part of the variable modifier ':@var@...@', which
# contains the variable name to use during the loop.

# Force the test results to be independent of the default value of this
# setting, which is 'yes' for NetBSD's usr.bin/make but 'no' for the bmake
# distribution and pkgsrc/devel/bmake.
.MAKE.SAVE_DOLLARS=	yes


# Before 2021-04-04, the name of the loop variable could be generated
# dynamically.  There was no practical use-case for this.
# Since var.c 1.907 from 2021-04-04, a '$' is no longer allowed in the
# variable name.
.if ${:Uone two three:@${:Ubar:S,b,v,}@+${var}+@} != "+one+ +two+ +three+"
.  error
.else
.  error
.endif


# ":::" is a very creative variable name, unlikely to occur in practice.
# The expression ${\:\:\:} would not work since backslashes can only
# be escaped in the modifiers, but not in the variable name, therefore
# the extra indirection via the modifier ':U'.
.if ${:U1 2 3:@:::@x${${:U\:\:\:}}y@} != "x1y x2y x3y"
.  error
.endif


# "@@" is another creative variable name.
.if ${:U1 2 3:@\@\@@x${@@}y@} != "x1y x2y x3y"
.  error
.endif


# In extreme cases, even the backslash can be used as variable name.
# It needs to be doubled though.
.if ${:U1 2 3:@\\@x${${:Ux:S,x,\\,}}y@} != "x1y x2y x3y"
.  error
.endif


# The variable name can technically be empty, and in this situation
# the variable value cannot be accessed since the empty "variable"
# is protected to always return an empty string.
.if ${:U1 2 3:@@x${}y@} != "xy xy xy"
.  error
.endif


# The :@ modifier resolves the variables from the replacement text once more
# than expected.  In particular, it resolves _all_ variables from the scope,
# and not only the loop variable (in this case v).
SRCS=		source
CFLAGS.source=	before
ALL_CFLAGS:=	${SRCS:@src@${CFLAGS.${src}}@}	# note the ':='
CFLAGS.source+=	after
.if ${ALL_CFLAGS} != "before"
.  error
.endif


# In the following example, the modifier ':@' expands the '$$' to '$'.  This
# means that when the resulting expression is evaluated, these resulting '$'
# will be interpreted as starting a subexpression.
#
# The d means direct reference, the i means indirect reference.
RESOLVE=	${RES1} $${RES1}
RES1=		1d${RES2} 1i$${RES2}
RES2=		2d${RES3} 2i$${RES3}
RES3=		3

.if ${RESOLVE:@v@w${v}w@} != "w1d2d3w w2i3w w1i2d3 2i\${RES3}w w1d2d3 2i\${RES3} 1i\${RES2}w"
.  error
.endif


# Until 2020-07-20, the variable name of the :@ modifier could end with one
# or two dollar signs, which were silently ignored.
# There's no point in allowing a dollar sign in that position.
# Since var.c 1.907 from 2021-04-04, a '$' is no longer allowed in the
# variable name.
.if ${1 2 3:L:@v$@($v)@} != "(1) (2) (3)"
.  error
.else
.  error
.endif
.if ${1 2 3:L:@v$$@($v)@} != "() () ()"
.  error
.else
.  error
.endif
.if ${1 2 3:L:@v$$$@($v)@} != "() () ()"
.  error
.else
.  error
.endif


# It may happen that there are nested :@ modifiers that use the same name for
# for the loop variable.  These modifiers influence each other.
#
# As of 2020-10-18, the :@ modifier is implemented by actually setting a
# variable in the scope of the expression and deleting it again after the
# loop.  This is different from the .for loops, which substitute the variable
# expression with ${:Uvalue}, leading to different unwanted side effects.
#
# To make the behavior more predictable, the :@ modifier should restore the
# loop variable to the value it had before the loop.  This would result in
# the string "1a b c1 2a b c2 3a b c3", making the two loops independent.
.if ${:U1 2 3:@i@$i${:Ua b c:@i@$i@}${i:Uu}@} != "1a b cu 2a b cu 3a b cu"
.  error
.endif

# During the loop, the variable is actually defined and nonempty.
# If the loop were implemented in the same way as the .for loop, the variable
# would be neither defined nor nonempty since all expressions of the form
# ${var} would have been replaced with ${:Uword} before evaluating them.
.if defined(var)
.  error
.endif
.if ${:Uword:@var@${defined(var):?def:undef} ${empty(var):?empty:nonempty}@} \
    != "def nonempty"
.  error
.endif
.if defined(var)
.  error
.endif

all: .PHONY
