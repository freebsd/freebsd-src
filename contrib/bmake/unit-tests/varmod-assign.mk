# $NetBSD: varmod-assign.mk,v 1.19 2024/01/07 11:42:22 rillig Exp $
#
# Tests for the obscure ::= variable modifiers, which perform variable
# assignments during evaluation, just like the = operator in C.

.if !make(target)

all:	mod-assign-empty
all:	mod-assign-parse
all:	mod-assign-shell-error

# In the following loop expression,
# the '::?=' modifier applies the assignment operator '?=' 3 times. The
# operator '?=' only has an effect for the first time, therefore the variable
# FIRST ends up with the value 1.
.if "${1 2 3:L:@i@${FIRST::?=$i}@} first=${FIRST}" != " first=1"
.  error
.endif

# In the following loop expression,
# the modifier '::=' applies the assignment operator '=' 3 times. The
# operator '=' overwrites the previous value, therefore the variable LAST ends
# up with the value 3.
.if "${1 2 3:L:@i@${LAST::=$i}@} last=${LAST}" != " last=3"
.  error
.endif

# In the following loop expression,
# the modifier '::+=' applies the assignment operator '+=' 3 times. The
# operator '+=' appends 3 times to the variable, therefore the variable
# APPENDED ends up with the value "1 2 3".
.if "${1 2 3:L:@i@${APPENDED::+=$i}@} appended=${APPENDED}" != " appended=1 2 3"
.  error
.endif

# In the following loop expression,
# the modifier '::!=' applies the assignment operator '!=' 3 times. Just as
# with the modifier '::=', the last value is stored in the RAN variable.
.if "${1 2 3:L:@i@${RAN::!=${i:%=echo '<%>';}}@} ran=${RAN}" != " ran=<3>"
.  error
.endif

# When a '::=' modifier is evaluated as part of an .if condition, it happens
# in the command line scope.
.if "${FIRST}, ${LAST}, ${APPENDED}, ${RAN}" != "1, 3, 1 2 3, <3>"
.  error
.endif

# Tests for nested assignments, which are hard to read and therefore seldom
# used in practice.

# The condition "1" is true, therefore THEN1 gets assigned a value,
# and the inner IT1 as well.  Nothing surprising here.
.if "${1:?${THEN1::=then1${IT1::=t1}}:${ELSE1::=else1${IE1::=e1}}} ${THEN1}${ELSE1}${IT1}${IE1}" != " then1t1"
.  error
.endif

# The condition "0" is false, therefore ELSE2 gets assigned a value,
# and the inner IE2 as well.  Nothing surprising here as well.
.if "${0:?${THEN2::=then2${IT2::=t2}}:${ELSE2::=else2${IE2::=e2}}} ${THEN2}${ELSE2}${IT2}${IE2}" != " else2e2"
.  error
.endif

# The same effects happen when the variables are defined elsewhere.
SINK3:=	${1:?${THEN3::=then3${IT3::=t3}}:${ELSE3::=else3${IE3::=e3}}} ${THEN3}${ELSE3}${IT3}${IE3}
SINK4:=	${0:?${THEN4::=then4${IT4::=t4}}:${ELSE4::=else4${IE4::=e4}}} ${THEN4}${ELSE4}${IT4}${IE4}
.if ${SINK3} != " then3t3"
.  error
.endif
.if ${SINK4} != " else4e4"
.  error
.endif

mod-assign-empty:
	# Assigning to the empty variable would obviously not work since that
	# variable is write-protected.  Therefore it is rejected early with a
	# "Bad modifier" message.
	@echo $@: ${::=value}

	# In this variant, it is not as obvious that the name of the
	# expression is empty.  Assigning to it is rejected as well, with the
	# same "Bad modifier" message.
	@echo $@: ${:Uvalue::=overwritten}

	# The :L modifier sets the value of the expression to its variable
	# name.  The name of the expression is "VAR", therefore assigning to
	# that variable works.
	@echo $@: ${VAR:L::=overwritten} VAR=${VAR}

mod-assign-parse:
	# The modifier for assignment operators starts with a ':'.
	# An 'x' after that is an invalid modifier.
	# expect: make: Unknown modifier ":x"
	@echo ${ASSIGN::x}

	# When parsing an assignment operator fails because the operator is
	# incomplete, make falls back to the SysV modifier.
	@echo ${SYSV::=sysv\:x}${SYSV::x=:y}

	@echo ${ASSIGN::=value	# missing closing brace

mod-assign-shell-error:
	# If the command succeeds, the variable is assigned.
	@${SH_OK::!= echo word; true } echo ok=${SH_OK}

	# If the command fails, the variable keeps its previous value.
	@${SH_ERR::=previous}
	@${SH_ERR::!= echo word; false } echo err=${SH_ERR}

# XXX: The ::= modifier expands its right-hand side exactly once.
# This differs subtly from normal assignments such as '+=' or '=', which copy
# their right-hand side literally.
APPEND.prev=		previous
APPEND.var=		${APPEND.prev}
APPEND.indirect=	indirect $${:Unot expanded}
APPEND.dollar=		$${APPEND.indirect}
.if ${APPEND.var::+=${APPEND.dollar}} != ""
.  error
.endif
.if ${APPEND.var} != "previous indirect \${:Unot expanded}"
.  error
.endif


# The assignment modifier can be used in an expression that is
# enclosed in parentheses.  In such a case, parsing stops at the first ')',
# not at the first '}'.
VAR=	previous
_:=	$(VAR::=current})
.if ${VAR} != "current}"
.  error
.endif


# Before var.c 1.888 from 2021-03-15, an expression using the modifier '::='
# expanded its variable name once too often during evaluation.  This was only
# relevant for variable names containing a '$' sign in their actual name, not
# the usual VAR.${param}.
.MAKEFLAGS: -dv
param=		twice
VARNAME=	VAR.$${param}	# Indirect variable name because of the '$',
				# to avoid difficult escaping rules.

${VARNAME}=	initial-value	# Sets 'VAR.${param}' to 'expanded'.
.if defined(VAR.twice)		# At this point, the '$$' is not expanded.
.  error
.endif
.if ${${VARNAME}::=assigned-value} # Here the variable name gets expanded once
.  error			# too often.
.endif
.if defined(VAR.twice)
.  error The variable name in the '::=' modifier is expanded once too often.
.endif
.if ${${VARNAME}} != "assigned-value"
.  error
.endif
.MAKEFLAGS: -d0


# Conditional directives are evaluated in command line scope.  An assignment
# modifier that creates a new variable creates it in the command line scope.
# Existing variables are updated in their previous scope, and environment
# variables are created in the global scope, as in other situations.
.MAKEFLAGS: CMD_CMD_VAR=cmd-value
CMD_GLOBAL_VAR=global-value
export CMD_ENV_VAR=env-value
.MAKEFLAGS: -dv
# expect-reset
# expect: Command: CMD_CMD_VAR = new-value
# expect: Global: CMD_GLOBAL_VAR = new-value
# expect: Global: CMD_ENV_VAR = new-value
# expect: Global: ignoring delete 'CMD_NEW_VAR' as it is not found
# expect: Command: CMD_NEW_VAR = new-value
.if ${CMD_CMD_VAR::=new-value} \
  || ${CMD_GLOBAL_VAR::=new-value} \
  || ${CMD_ENV_VAR::=new-value} \
  || "${CMD_NEW_VAR::=new-value}"
.  error
.endif
.MAKEFLAGS: -d0

# Run the 'target' test in a separate sub-make, with reduced debug logging.
all: run-target
run-target: .PHONY
	@${MAKE} -r -f ${MAKEFILE} -dv target 2>&1 | grep ': TARGET_'

.else # make(target)

# The commands of a target are evaluated in target scope.  An assignment
# modifier that creates a new variable creates it in the target scope.
# Existing variables are updated in their previous scope, and environment
# variables are created in the global scope, as in other situations.
#
# expect: target: TARGET_TARGET_VAR = new-value
# expect: Global: TARGET_GLOBAL_VAR = new-value
# expect: Global: TARGET_ENV_VAR = new-value
# expect: target: TARGET_NEW_VAR = new-value
.MAKEFLAGS: TARGET_CMD_VAR=cmd-value
TARGET_GLOBAL_VAR=global-value
export TARGET_ENV_VAR=env-value
target: .PHONY TARGET_TARGET_VAR=target-value
	: ${TARGET_TARGET_VAR::=new-value}
	: ${TARGET_CMD_VAR::=new-value}
	: ${TARGET_GLOBAL_VAR::=new-value}
	: ${TARGET_ENV_VAR::=new-value}
	: ${TARGET_NEW_VAR::=new-value}

.endif
