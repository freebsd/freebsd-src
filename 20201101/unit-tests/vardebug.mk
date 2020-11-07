# $NetBSD: vardebug.mk,v 1.6 2020/10/31 13:15:10 rillig Exp $
#
# Demonstrates the debugging output for var.c.

.MAKEFLAGS: -dv FROM_CMDLINE=

VAR=		added		# VarAdd
VAR=		overwritten	# Var_Set
.undef VAR			# Var_Delete (found)
.undef VAR			# Var_Delete (not found)

# The variable with the empty name cannot be set at all.
${:U}=		empty name	# Var_Set
${:U}+=		empty name	# Var_Append

FROM_CMDLINE=	overwritten	# Var_Set (ignored)

VAR=		1
VAR+=		2
VAR+=		3

.if ${VAR:M[2]}			# ModifyWord_Match
.endif
.if ${VAR:N[2]}			# ModifyWord_NoMatch (no debug output)
.endif

.if ${VAR:S,2,two,}		# ParseModifierPart
.endif

.if ${VAR:Q}			# VarQuote
.endif

.if ${VAR:tu:tl:Q}		# ApplyModifiers
.endif

# ApplyModifiers, "Got ..."
.if ${:Uvalue:${:UM*e}:Mvalu[e]}
.endif

.undef ${:UVAR}			# Var_Delete

# When ApplyModifiers results in an error, this appears in the debug log
# as "is error", without surrounding quotes.
.if ${:Uvariable:unknown}
.endif

# XXX: The error message is "Malformed conditional", which is wrong.
# The condition is syntactically fine, it just contains an undefined variable.
#
# There is a specialized error message for "Undefined variable", but as of
# 2020-08-08, that is not covered by any unit tests.  It might even be
# unreachable.
.if ${UNDEFINED}
.endif

# By default, .SHELL is not defined and thus can be set.  As soon as it is
# accessed, it is initialized in the command line context (during VarFind),
# where it is set to read-only.  Assigning to it is ignored.
.MAKEFLAGS: .SHELL=overwritten

.MAKEFLAGS: -d0

all:
	@:
