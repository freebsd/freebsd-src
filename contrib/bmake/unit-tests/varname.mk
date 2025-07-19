# $NetBSD: varname.mk,v 1.18 2025/06/28 22:39:29 rillig Exp $
#
# Tests for variable names.

.MAKEFLAGS: -dv

# In a variable assignment, braces are allowed in the variable name, but they
# must be balanced.  Parentheses and braces may be mixed.
VAR{{{}}}=	3 braces
.if "${VAR{{{}}}}" != "3 braces"
.  error
.endif

# In expressions, the parser works differently.  It doesn't treat
# braces and parentheses equally, therefore the first closing brace already
# marks the end of the variable name.
VARNAME=	VAR(((
${VARNAME}=	3 open parentheses
.if "${VAR(((}}}}" != "3 open parentheses}}}"
.  error
.endif

# In the above test, the variable name is constructed indirectly.  Neither
# of the following expressions produces the intended effect.
#
# This is not a variable assignment since the parentheses and braces are not
# balanced.  At the end of the line, there are still 3 levels open, which
# means the variable name is not finished.
# expect+2: Missing ")" in archive specification
# expect+1: Error in archive specification: "VAR"
${:UVAR(((}=	try1
# On the left-hand side of a variable assignments, the backslash is not parsed
# as an escape character, therefore the parentheses still count to the nesting
# level, which at the end of the line is still 3.  Therefore this is not a
# variable assignment as well.
# expect+1: Invalid line "${:UVAR\(\(\(}=	try2", expanded to "VAR\(\(\(=	try2"
${:UVAR\(\(\(}=	try2
# To assign to a variable with an arbitrary name, the variable name has to
# come from an external source, not the text that is parsed in the assignment
# itself.  This is exactly the reason why further above, the indirect
# ${VARNAME} works, while all other attempts fail.
${VARNAME}=	try3

.MAKEFLAGS: -d0

# All variable names of a scope are stored in the same hash table, using a
# simple hash function.  Ensure that HashTable_Find handles collisions
# correctly and that the correct variable is looked up.  The strings "0x" and
# "1Y" have the same hash code, as 31 * '0' + 'x' == 31 * '1' + 'Y'.
V.0x=	0x
V.1Y=	1Y
.if ${V.0x} != "0x" || ${V.1Y} != "1Y"
.  error
.endif

# The string "ASDZguv", when used as a prefix of a variable name, keeps the
# hash code unchanged, its own hash code is 0.
ASDZguvV.0x=	0x
ASDZguvV.1Y=	1Y
.if ${ASDZguvV.0x} != "0x"
.  error
.elif ${ASDZguvV.1Y} != "1Y"
.  error
.endif

# Ensure that variables with the same hash code whose name is a prefix of the
# other can be accessed.  In this case, the shorter variable name is defined
# first to make it appear later in the bucket of the hash table.
ASDZguv=	once
ASDZguvASDZguv=	twice
.if ${ASDZguv} != "once"
.  error
.elif ${ASDZguvASDZguv} != "twice"
.  error
.endif

# Ensure that variables with the same hash code whose name is a prefix of the
# other can be accessed.  In this case, the longer variable name is defined
# first to make it appear later in the bucket of the hash table.
ASDZguvASDZguv.param=	twice
ASDZguv.param=		once
.if ${ASDZguv.param} != "once"
.  error
.elif ${ASDZguvASDZguv.param} != "twice"
.  error
.endif


# Warn about expressions in the style of GNU make, as these would silently
# expand to an empty string instead.
#
# https://pubs.opengroup.org/onlinepubs/9799919799/utilities/make.html says:
#	a macro name shall not contain an <equals-sign>, <blank>, or control
#	character.
#
GNU_MAKE_IF=	$(if ${HAVE_STRLEN},yes,no)
# expect+1: warning: Invalid character " " in variable name "if ,yes,no"
.if ${GNU_MAKE_IF} != ""
.  error
.endif
#
# This requirement needs to be ignored for expressions with a ":L" or ":?:"
# modifier, as these modifiers rely on arbitrary characters in the expression
# name.
.if ${"left" == "right":?equal:unequal} != "unequal"
.  error
.endif
#
# In fact, this requirement is ignored for any expression that has a modifier.
# In this indirect case, though, the expression with the space in the name is
# a nested expression, so the ":U" modifier doesn't affect the warning.
# expect+1: warning: Invalid character " " in variable name "if ,yes,no"
.if ${GNU_MAKE_IF:Ufallback} != ""
.  error
.endif
#
# A modifier in a nested expression does not affect the warning.
GNU_MAKE_IF_EXPR=	$(if ${HAVE_STRLEN},${HEADERS:.h=.c},)
# expect+1: warning: Invalid character " " in variable name "if ,,"
.if ${GNU_MAKE_IF_EXPR} != ""
.  error
.endif
#
# When the GNU make expression contains a colon, chances are good that the
# colon is interpreted as an unknown modifier.
GNU_MAKE_IF_MODIFIER=	$(if ${HAVE_STRLEN},answer:yes,answer:no)
# expect+1: Unknown modifier ":yes,answer"
.if ${GNU_MAKE_IF_MODIFIER} != "no)"
.  error
.endif
#
# If the variable name contains a non-printable character, the warning
# contains the numeric character value instead, to prevent control sequences
# in the output.
CONTROL_CHARACTER=	${:U a b:ts\t}
# expect+2: warning: Invalid character "\x09" in variable name "a	b"
# expect+1: Variable "a	b" is undefined
.if ${${CONTROL_CHARACTER}} != ""
.endif
#
# For now, only whitespace generates a warning, non-ASCII characters don't.
UMLAUT=		ÄÖÜ
# expect+1: Variable "ÄÖÜ" is undefined
.if ${${UMLAUT}} != ""
.endif
