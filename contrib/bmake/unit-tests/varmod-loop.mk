# $NetBSD: varmod-loop.mk,v 1.24 2023/11/19 21:47:52 rillig Exp $
#
# Tests for the expression modifier ':@var@body@', which replaces each word of
# the expression with the expanded body, which may contain references to the
# variable 'var'.  For example, '${1 2 3:L:@word@<${word}>@}' encloses each
# word in angle quotes, resulting in '<1> <2> <3>'.
#
# The variable name can be chosen freely, except that it must not contain a
# '$'.  For simplicity and readability, variable names should only use the
# characters 'A-Za-z0-9'.
#
# The body may contain subexpressions in the form '${...}' or '$(...)'.  These
# subexpressions differ from everywhere else in makefiles in that the parser
# only scans '${...}' for balanced '{' and '}', likewise for '$(...)'.  Any
# other '$' is left as-is during parsing.  Later, when the body is expanded
# for each word, each '$$' is interpreted as a single '$', and the remaining
# '$' are interpreted as expressions, like when evaluating a regular variable.

# Force the test results to be independent of the default value of this
# setting, which is 'yes' for NetBSD's usr.bin/make but 'no' for the bmake
# distribution and pkgsrc/devel/bmake.
.MAKE.SAVE_DOLLARS=	yes

all: varname-overwriting-target
all: mod-loop-dollar

varname-overwriting-target:
	# Even "@" works as a variable name since the variable is installed
	# in the "current" scope, which in this case is the one from the
	# target.  Because of this, after the loop has finished, '$@' is
	# undefined.  This is something that make doesn't expect, this may
	# even trigger an assertion failure somewhere.
	@echo :$@: :${:U1 2 3:@\@@x${@}y@}: :$@:


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
# loop.  This is different from the .for loops, which substitute the
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
# prevented by VARE_EVAL_KEEP_DOLLAR, but that flag is usually removed
# before expanding subexpressions.  See ApplyModifier_Loop and
# ParseModifierPart for examples.
#
.MAKEFLAGS: -dcp
USE_8_DOLLARS=	${:U1:@var@${8_DOLLARS}@} ${8_DOLLARS} $$$$$$$$
.if ${USE_8_DOLLARS} != "\$\$\$\$ \$\$\$\$ \$\$\$\$"
.  error
.endif
#
SUBST_CONTAINING_LOOP:= ${USE_8_DOLLARS}
# The ':=' assignment operator evaluates the variable value using the mode
# VARE_KEEP_DOLLAR_UNDEF, which means that some dollar signs are preserved,
# but not all.  The dollar signs in the top-level expression and in the
# indirect ${8_DOLLARS} are preserved.
#
# The variable modifier :@var@ does not preserve the dollar signs though, no
# matter in which context it is evaluated.  What happens in detail is:
# First, the modifier part "${8_DOLLARS}" is parsed without expanding it.
# Next, each word of the value is expanded on its own, and at this moment
# in ApplyModifier_Loop, the flag keepDollar is not passed down to
# ModifyWords, resulting in "$$$$" for the first word of USE_8_DOLLARS.
#
# The remaining words of USE_8_DOLLARS are not affected by any variable
# modifier and are thus expanded with the flag keepDollar in action.
# The variable SUBST_CONTAINING_LOOP therefore gets assigned the raw value
# "$$$$ $$$$$$$$ $$$$$$$$".
#
# The expression in the condition then expands this raw stored value
# once, resulting in "$$ $$$$ $$$$".  The effects from VARE_KEEP_DOLLAR no
# longer take place since they had only been active during the evaluation of
# the variable assignment.
.if ${SUBST_CONTAINING_LOOP} != "\$\$ \$\$\$\$ \$\$\$\$"
.  error
.endif
.MAKEFLAGS: -d0

# After looping over the words of the expression, the loop variable gets
# undefined.  The modifier ':@' uses an ordinary global variable for this,
# which is different from the '.for' loop, which replaces ${var} with
# ${:Uvalue} in the body of the loop.  This choice of implementation detail
# can be used for a nasty side effect.  The expression ${:U:@VAR@@} evaluates
# to an empty string, plus it undefines the variable 'VAR'.  This is the only
# possibility to undefine a global variable during evaluation.
GLOBAL=		before-global
RESULT:=	${:U${GLOBAL} ${:U:@GLOBAL@@} ${GLOBAL:Uundefined}}
.if ${RESULT} != "before-global  undefined"
.  error
.endif

# The above side effect of undefining a variable from a certain scope can be
# further combined with the otherwise undocumented implementation detail that
# the argument of an '.if' directive is evaluated in cmdline scope.  Putting
# these together makes it possible to undefine variables from the cmdline
# scope, something that is not possible in a straight-forward way.
.MAKEFLAGS: CMDLINE=cmdline
.if ${:U${CMDLINE}${:U:@CMDLINE@@}} != "cmdline"
.  error
.endif
# Now the cmdline variable got undefined.
.if ${CMDLINE} != "cmdline"
.  error
.endif
# At this point, it still looks as if the cmdline variable were defined,
# since the value of CMDLINE is still "cmdline".  That impression is only
# superficial though, the cmdline variable is actually deleted.  To
# demonstrate this, it is now possible to override its value using a global
# variable, something that was not possible before:
CMDLINE=	global
.if ${CMDLINE} != "global"
.  error
.endif
# Now undefine that global variable again, to get back to the original value.
.undef CMDLINE
.if ${CMDLINE} != "cmdline"
.  error
.endif
# What actually happened is that when CMDLINE was set by the '.MAKEFLAGS'
# target in the cmdline scope, that same variable was exported to the
# environment, see Var_SetWithFlags.
.unexport CMDLINE
.if ${CMDLINE} != "cmdline"
.  error
.endif
# The above '.unexport' has no effect since UnexportVar requires a global
# variable of the same name to be defined, otherwise nothing is unexported.
CMDLINE=	global
.unexport CMDLINE
.undef CMDLINE
.if ${CMDLINE} != "cmdline"
.  error
.endif
# This still didn't work since there must not only be a global variable, the
# variable must be marked as exported as well, which it wasn't before.
CMDLINE=	global
.export CMDLINE
.unexport CMDLINE
.undef CMDLINE
.if ${CMDLINE:Uundefined} != "undefined"
.  error
.endif
# Finally the variable 'CMDLINE' from the cmdline scope is gone, and all its
# traces from the environment are gone as well.  To do that, a global variable
# had to be defined and exported, something that is far from obvious.  To
# recap, here is the essence of the above story:
.MAKEFLAGS: CMDLINE=cmdline	# have a cmdline + environment variable
.if ${:U:@CMDLINE@@}}		# undefine cmdline, keep environment
.endif
CMDLINE=	global		# needed for deleting the environment
.export CMDLINE			# needed for deleting the environment
.unexport CMDLINE		# delete the environment
.undef CMDLINE			# delete the global helper variable
.if ${CMDLINE:Uundefined} != "undefined"
.  error			# 'CMDLINE' is gone now from all scopes
.endif


# In the loop body text of the ':@' modifier, a literal '$' is written as '$$',
# not '\$'.  In the following example, each '$$' turns into a single '$',
# except for '$i', which is replaced with the then-current value '1' of the
# iteration variable.
#
# See parse-var.mk, keyword 'BRACE_GROUP'.
all: varmod-loop-literal-dollar
varmod-loop-literal-dollar: .PHONY
	: ${:U1:@i@ t=$$(( $${t:-0} + $i ))@}


# When parsing the loop body, each '\$', '\@' and '\\' is unescaped to '$',
# '@' and '\', respectively; all other backslashes are retained.
#
# In practice, the '$' is not escaped as '\$', as there is a second round of
# unescaping '$$' to '$' later when the loop body is expanded after setting the
# iteration variable.
#
# After the iteration variable has been set, the loop body is expanded with
# this unescaping, regardless of whether .MAKE.SAVE_DOLLARS is set or not:
#	$$			a literal '$'
#	$x, ${var}, $(var)	a nested expression
#	any other character	itself
all: escape-modifier
escape-modifier: .PHONY
	# In the first round, '\$ ' is unescaped to '$ ', and since the
	# variable named ' ' is not defined, the expression '$ ' expands to an
	# empty string.
	# expect: :  dollar=end
	: ${:U1:@i@ dollar=\$ end@}

	# Like in other modifiers, '\ ' is preserved, since ' ' is not one of
	# the characters that _must_ be escaped.
	# expect: :  backslash=\ end
	: ${:U1:@i@ backslash=\ end@}

	# expect: :  dollar=$ at=@ backslash=\ end
	: ${:U1:@i@ dollar=\$\$ at=\@ backslash=\\ end@}
	# expect: :  dollar=$$ at=@@ backslash=\\ end
	: ${:U1:@i@ dollar=\$\$\$\$ at=\@\@ backslash=\\\\ end@}
	# expect: :  dollar=$$ at=@@ backslash=\\ end
	: ${:U1:@i@ dollar=$$$$ at=\@\@ backslash=\\\\ end@}

all: .PHONY
