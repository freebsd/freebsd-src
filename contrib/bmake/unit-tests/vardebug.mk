# $NetBSD: vardebug.mk,v 1.18 2025/03/29 19:08:52 rillig Exp $
#
# Demonstrates the debugging output for var.c.

.MAKEFLAGS: -dv FROM_CMDLINE=

# expect: Global: VAR = added
VAR=		added		# VarAdd
# expect: Global: VAR = overwritten
VAR=		overwritten	# Var_Set
# expect: Global: delete VAR
.undef VAR
# expect: Global: ignoring delete 'VAR' as it is not found
.undef VAR

# The variable with the empty name cannot be set at all.
# expect: Global: ignoring ' = empty name' as the variable name '${:U}' expands to empty
${:U}=		empty name	# Var_Set
# expect: Global: ignoring ' += empty name' as the variable name '${:U}' expands to empty
${:U}+=		empty name	# Var_Append

FROM_CMDLINE=	overwritten	# Var_Set (ignored)

# expect: Global: VAR = 1
VAR=		1
# expect: Global: VAR = 1 2
VAR+=		2
# expect: Global: VAR = 1 2 3
VAR+=		3

# expect: Pattern for ':M' is "[2]"
# expect: Result of ${VAR:M[2]} is "2"
.if ${VAR:M[2]}			# ModifyWord_Match
.endif
# expect: Pattern for ':N' is "[2]"
# expect: Result of ${VAR:N[2]} is "1 3"
.if ${VAR:N[2]}			# ModifyWord_NoMatch
.endif

.if ${VAR:S,2,two,}		# ParseModifierPart
.endif

# expect: Result of ${VAR:Q} is "1\ 2\ 3"
.if ${VAR:Q}			# VarQuote
.endif

.if ${VAR:tu:tl:Q}		# ApplyModifiers
.endif

# ApplyModifiers, "Got ..."
# expect: Result of ${:Mvalu[e]} is "value" (eval, defined)
.if ${:Uvalue:${:UM*e}:Mvalu[e]}
.endif

# expect: Global: delete VAR
.undef ${:UVAR}			# Var_Delete

# expect+1: Unknown modifier ":unknown"
.if ${:Uvariable:unknown}
.endif

# expect+1: Variable "UNDEFINED" is undefined
.if ${UNDEFINED}
.endif

# By default, .SHELL is not defined and thus can be set.  As soon as it is
# accessed, it is initialized in the command line scope (during VarFind),
# where it is set to read-only.  Assigning to it is ignored.
# expect: Command: ignoring '.SHELL = overwritten' as it is read-only
.MAKEFLAGS: .SHELL=overwritten

DYN = ${:U$@} $@ ${@}
# expect: Global: DYN = $(.TARGET) $(.TARGET) ${@}
DYN := ${DYN}

.MAKEFLAGS: -d0
