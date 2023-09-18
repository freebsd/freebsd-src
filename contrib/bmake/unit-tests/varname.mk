# $NetBSD: varname.mk,v 1.13 2023/08/19 11:09:02 rillig Exp $
#
# Tests for special variables, such as .MAKE or .PARSEDIR.
# And for variable names in general.

.MAKEFLAGS: -dv

# In variable names, braces are allowed, but they must be balanced.
# Parentheses and braces may be mixed.
VAR{{{}}}=	3 braces
.if "${VAR{{{}}}}" != "3 braces"
.  error
.endif

# In variable expressions, the parser works differently.  It doesn't treat
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
# expect+2: Error in archive specification: "VAR"
# expect+1: No closing parenthesis in archive specification
${:UVAR(((}=	try1
# On the left-hand side of a variable assignments, the backslash is not parsed
# as an escape character, therefore the parentheses still count to the nesting
# level, which at the end of the line is still 3.  Therefore this is not a
# variable assignment as well.
# expect+1: Invalid line '${:UVAR\(\(\(}=	try2', expanded to 'VAR\(\(\(=	try2'
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

all:
