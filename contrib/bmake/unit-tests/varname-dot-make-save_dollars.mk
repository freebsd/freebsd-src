# $NetBSD: varname-dot-make-save_dollars.mk,v 1.7 2021/12/03 18:43:52 rillig Exp $
#
# Tests for the special .MAKE.SAVE_DOLLARS variable, which controls whether
# the assignment operator ':=' converts '$$' to a single '$' or keeps it
# as-is.
#
# See also:
#	var-op-expand.mk	for ':=' in general
#	varmisc.mk		for parsing the boolean values

# Initially, the variable .MAKE.SAVE_DOLLARS is undefined. At this point the
# behavior of the assignment operator ':=' depends.  NetBSD's usr.bin/make
# preserves the '$$' as-is, while the bmake distribution replaces '$$' with
# '$'.
.if ${.MAKE.SAVE_DOLLARS:Uundefined} != "undefined"
.  error
.endif


# When dollars are preserved, this setting not only applies to literal
# dollars, but also to those that come indirectly from other expressions.
DOLLARS=		$$$$$$$$
.MAKE.SAVE_DOLLARS=	yes
VAR:=			${DOLLARS}
# The reduction from 8 '$' to 4 '$' happens when ${VAR} is evaluated in the
# condition; .MAKE.SAVE_DOLLARS only applies at the moment where the
# assignment is performed using ':='.
.if ${VAR} != "\$\$\$\$"
.  error
.endif

# When dollars are preserved, this setting not only applies to literal
# dollars, but also to those that come indirectly from other expressions.
DOLLARS=		$$$$$$$$
.MAKE.SAVE_DOLLARS=	no
VAR:=			${DOLLARS}
.if ${VAR} != "\$\$"
.  error
.endif

# The 'yes' preserves the dollars from the literal.
.MAKE.SAVE_DOLLARS=	yes
VAR:=			$$$$$$$$
.if ${VAR} != "\$\$\$\$"
.  error
.endif

# The 'no' converts each '$$' to '$'.
.MAKE.SAVE_DOLLARS=	no
VAR:=			$$$$$$$$
.if ${VAR} != "\$\$"
.  error
.endif

# It's even possible to change the dollar interpretation in the middle of
# evaluating an expression, but there is no practical need for it.
.MAKE.SAVE_DOLLARS=	no
VAR:=		$$$$-${.MAKE.SAVE_DOLLARS::=yes}-$$$$
.if ${VAR} != "\$--\$\$"
.  error
.endif

# The '$' from the ':U' expressions do not appear as literal '$$' to the
# parser (no matter whether directly or indirectly), they only appear as '$$'
# in the value of an expression, therefore .MAKE.SAVE_DOLLARS doesn't apply
# here.
.MAKE.SAVE_DOLLARS=	no
VAR:=		${:U\$\$\$\$}-${.MAKE.SAVE_DOLLARS::=yes}-${:U\$\$\$\$}
.if ${VAR} != "\$\$--\$\$"
.  error
.endif

# Undefining .MAKE.SAVE_DOLLARS does not have any effect, in particular it
# does not restore the default behavior.
.MAKE.SAVE_DOLLARS=	no
.undef .MAKE.SAVE_DOLLARS
VAR:=		$$$$$$$$
.if ${VAR} != "\$\$"
.  error
.endif

# Undefining .MAKE.SAVE_DOLLARS does not have any effect, in particular it
# does not restore the default behavior.
.MAKE.SAVE_DOLLARS=	yes
.undef .MAKE.SAVE_DOLLARS
VAR:=		$$$$$$$$
.if ${VAR} != "\$\$\$\$"
.  error
.endif

# The variable '.MAKE.SAVE_DOLLARS' not only affects literal '$$' on the
# right-hand side of the assignment operator ':=', it also affects dollars
# in indirect expressions.
#
# In this example, it affects the command in CMD itself, not the result of
# running that command.
.MAKE.SAVE_DOLLARS=	no
CMD=			echo '$$$$$$$$'
VAR:=			${CMD:sh}
.if ${VAR} != "\$\$"
.  error
.endif

.MAKE.SAVE_DOLLARS=	yes
CMD=			echo '$$$$$$$$'
VAR:=			${CMD:sh}
.if ${VAR} != "\$\$\$\$"
.  error
.endif


# In the modifier ':@var@body@', .MAKE.SAVE_DOLLARS does not affect the body.
# In both cases, each '$$' is replaced with a single '$', no matter whether
# directly or indirectly via another expression.
.MAKE.SAVE_DOLLARS=	no
DOLLARS=		$$$$$$$$
VAR:=			${word:L:@word@$$$$$$$$-${DOLLARS}@}
.if ${VAR} != "\$\$-\$\$"
.  error
.endif

.MAKE.SAVE_DOLLARS=	yes
DOLLARS=		$$$$$$$$
VAR:=			${word:L:@word@$$$$$$$$-${DOLLARS}@}
.if ${VAR} != "\$\$-\$\$"
.  error
.endif


all:
