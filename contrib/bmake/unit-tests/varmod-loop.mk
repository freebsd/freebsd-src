# $NetBSD: varmod-loop.mk,v 1.2 2020/08/16 12:30:45 rillig Exp $
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
	# ":::" is a very creative variable name, unlikely in practice
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
