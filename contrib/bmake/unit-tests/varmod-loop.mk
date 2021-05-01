# $NetBSD: varmod-loop.mk,v 1.9 2021/02/04 21:42:47 rillig Exp $
#
# Tests for the :@var@...${var}...@ variable modifier.

.MAKE.SAVE_DOLLARS=	yes

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
# In particular, it resolves _all_ variables from the scope, and not only
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

# Demonstrate that it is possible to generate dollar signs using the
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

# Assignment using the ':=' operator, combined with the :@var@ modifier
#
8_DOLLARS=	$$$$$$$$
# This string literal is written with 8 dollars, and this is saved as the
# variable value.  But as soon as this value is evaluated, it goes through
# Var_Subst, which replaces each '$$' with a single '$'.  This could be
# prevented by VARE_KEEP_DOLLAR, but that flag is usually removed before
# expanding subexpressions.  See ApplyModifier_Loop and ParseModifierPart
# for examples.
#
.MAKEFLAGS: -dcp
USE_8_DOLLARS=	${:U1:@var@${8_DOLLARS}@} ${8_DOLLARS} $$$$$$$$
.if ${USE_8_DOLLARS} != "\$\$\$\$ \$\$\$\$ \$\$\$\$"
.  error
.endif
#
SUBST_CONTAINING_LOOP:= ${USE_8_DOLLARS}
# The ':=' assignment operator evaluates the variable value using the flag
# VARE_KEEP_DOLLAR, which means that some dollar signs are preserved, but not
# all.  The dollar signs in the top-level expression and in the indirect
# ${8_DOLLARS} are preserved.
#
# The variable modifier :@var@ does not preserve the dollar signs though, no
# matter in which context it is evaluated.  What happens in detail is:
# First, the modifier part "${8_DOLLARS}" is parsed without expanding it.
# Next, each word of the value is expanded on its own, and at this moment
# in ApplyModifier_Loop, the VARE_KEEP_DOLLAR flag is not passed down to
# ModifyWords, resulting in "$$$$" for the first word of USE_8_DOLLARS.
#
# The remaining words of USE_8_DOLLARS are not affected by any variable
# modifier and are thus expanded with the flag VARE_KEEP_DOLLAR in action.
# The variable SUBST_CONTAINING_LOOP therefore gets assigned the raw value
# "$$$$ $$$$$$$$ $$$$$$$$".
#
# The variable expression in the condition then expands this raw stored value
# once, resulting in "$$ $$$$ $$$$".  The effects from VARE_KEEP_DOLLAR no
# longer take place since they had only been active during the evaluation of
# the variable assignment.
.if ${SUBST_CONTAINING_LOOP} != "\$\$ \$\$\$\$ \$\$\$\$"
.  error
.endif
.MAKEFLAGS: -d0
