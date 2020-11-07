# $NetBSD: varmod-loop.mk,v 1.5 2020/10/31 12:34:03 rillig Exp $
#
# Tests for the :@var@...${var}...@ variable modifier.

all: mod-loop-varname
all: mod-loop-resolve
all: mod-loop-varname-dollar
all: mod-loop-dollar

# In the :@ modifier, the name of the loop variable can even be generated
# dynamically.  There's no practical use-case for this, and hopefully nobody
# will ever depend on this, but technically it's possible.
# Therefore, in -dL mode, this is forbidden, see lint.mk.
mod-loop-varname:
	@echo :${:Uone two three:@${:Ubar:S,b,v,}@+${var}+@:Q}:

	# ":::" is a very creative variable name, unlikely in practice.
	# The expression ${\:\:\:} would not work since backslashes can only
	# be escaped in the modifiers, but not in the variable name.
	@echo :${:U1 2 3:@:::@x${${:U\:\:\:}}y@}:

	# "@@" is another creative variable name.
	@echo :${:U1 2 3:@\@\@@x${@@}y@}:

	# Even "@" works as a variable name since the variable is installed
	# in the "current" scope, which in this case is the one from the
	# target.
	@echo :$@: :${:U1 2 3:@\@@x${@}y@}: :$@:

	# In extreme cases, even the backslash can be used as variable name.
	# It needs to be doubled though.
	@echo :${:U1 2 3:@\\@x${${:Ux:S,x,\\,}}y@}:

	# The variable name can technically be empty, and in this situation
	# the variable value cannot be accessed since the empty variable is
	# protected to always return an empty string.
	@echo empty: :${:U1 2 3:@@x${}y@}:

# The :@ modifier resolves the variables a little more often than expected.
# In particular, it resolves _all_ variables from the context, and not only
# the loop variable (in this case v).
#
# The d means direct reference, the i means indirect reference.
RESOLVE=	${RES1} $${RES1}
RES1=		1d${RES2} 1i$${RES2}
RES2=		2d${RES3} 2i$${RES3}
RES3=		3

mod-loop-resolve:
	@echo $@:${RESOLVE:@v@w${v}w@:Q}:

# Until 2020-07-20, the variable name of the :@ modifier could end with one
# or two dollar signs, which were silently ignored.
# There's no point in allowing a dollar sign in that position.
mod-loop-varname-dollar:
	@echo $@:${1 2 3:L:@v$@($v)@:Q}.
	@echo $@:${1 2 3:L:@v$$@($v)@:Q}.
	@echo $@:${1 2 3:L:@v$$$@($v)@:Q}.

# Demonstrate that it is possible to generate dollar characters using the
# :@ modifier.
#
# These are edge cases that could have resulted in a parse error as well
# since the $@ at the end could have been interpreted as a variable, which
# would mean a missing closing @ delimiter.
mod-loop-dollar:
	@echo $@:${:U1:@word@${word}$@:Q}:
	@echo $@:${:U2:@word@$${word}$$@:Q}:
	@echo $@:${:U3:@word@$$${word}$$$@:Q}:
	@echo $@:${:U4:@word@$$$${word}$$$$@:Q}:
	@echo $@:${:U5:@word@$$$$${word}$$$$$@:Q}:
	@echo $@:${:U6:@word@$$$$$${word}$$$$$$@:Q}:

# It may happen that there are nested :@ modifiers that use the same name for
# for the loop variable.  These modifiers influence each other.
#
# As of 2020-10-18, the :@ modifier is implemented by actually setting a
# variable in the context of the expression and deleting it again after the
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
