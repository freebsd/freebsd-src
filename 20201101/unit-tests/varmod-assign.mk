# $NetBSD: varmod-assign.mk,v 1.8 2020/10/18 21:37:24 rillig Exp $
#
# Tests for the obscure ::= variable modifiers, which perform variable
# assignments during evaluation, just like the = operator in C.

all:	mod-assign
all:	mod-assign-nested
all:	mod-assign-empty
all:	mod-assign-parse
all:	mod-assign-shell-error

mod-assign:
	# The ::?= modifier applies the ?= assignment operator 3 times.
	# The ?= operator only has an effect for the first time, therefore
	# the variable FIRST ends up with the value 1.
	@echo $@: ${1 2 3:L:@i@${FIRST::?=$i}@} first=${FIRST}.

	# The ::= modifier applies the = assignment operator 3 times.
	# The = operator overwrites the previous value, therefore the
	# variable LAST ends up with the value 3.
	@echo $@: ${1 2 3:L:@i@${LAST::=$i}@} last=${LAST}.

	# The ::+= modifier applies the += assignment operator 3 times.
	# The += operator appends 3 times to the variable, therefore
	# the variable APPENDED ends up with the value "1 2 3".
	@echo $@: ${1 2 3:L:@i@${APPENDED::+=$i}@} appended=${APPENDED}.

	# The ::!= modifier applies the != assignment operator 3 times.
	# The side effects of the shell commands are visible in the output.
	# Just as with the ::= modifier, the last value is stored in the
	# RAN variable.
	@echo $@: ${echo.1 echo.2 echo.3:L:@i@${RAN::!=${i:C,.*,&; & 1>\&2,:S,., ,g}}@} ran:${RAN}.

	# The assignments happen in the global scope and thus are
	# preserved even after the shell command has been run.
	@echo $@: global: ${FIRST:Q}, ${LAST:Q}, ${APPENDED:Q}, ${RAN:Q}.

mod-assign-nested:
	# The condition "1" is true, therefore THEN1 gets assigned a value,
	# and IT1 as well.  Nothing surprising here.
	@echo $@: ${1:?${THEN1::=then1${IT1::=t1}}:${ELSE1::=else1${IE1::=e1}}}${THEN1}${ELSE1}${IT1}${IE1}

	# The condition "0" is false, therefore ELSE1 gets assigned a value,
	# and IE1 as well.  Nothing surprising here as well.
	@echo $@: ${0:?${THEN2::=then2${IT2::=t2}}:${ELSE2::=else2${IE2::=e2}}}${THEN2}${ELSE2}${IT2}${IE2}

	# The same effects happen when the variables are defined elsewhere.
	@echo $@: ${SINK3:Q}
	@echo $@: ${SINK4:Q}
SINK3:=	${1:?${THEN3::=then3${IT3::=t3}}:${ELSE3::=else3${IE3::=e3}}}${THEN3}${ELSE3}${IT3}${IE3}
SINK4:=	${0:?${THEN4::=then4${IT4::=t4}}:${ELSE4::=else4${IE4::=e4}}}${THEN4}${ELSE4}${IT4}${IE4}

mod-assign-empty:
	# Assigning to the empty variable would obviously not work since that
	# variable is write-protected.  Therefore it is rejected early with a
	# "Bad modifier" message.
	#
	# XXX: The error message is hard to read since the variable name is
	# empty.  This leads to a trailing space in the error message.
	@echo $@: ${::=value}

	# In this variant, it is not as obvious that the name of the
	# expression is empty.  Assigning to it is rejected as well, with the
	# same "Bad modifier" message.
	#
	# XXX: The error message is hard to read since the variable name is
	# empty.  This leads to a trailing space in the error message.
	@echo $@: ${:Uvalue::=overwritten}

	# The :L modifier sets the value of the expression to its variable
	# name.  The name of the expression is "VAR", therefore assigning to
	# that variable works.
	@echo $@: ${VAR:L::=overwritten} VAR=${VAR}

mod-assign-parse:
	# The modifier for assignment operators starts with a ':'.
	# An 'x' after that is an invalid modifier.
	@echo ${ASSIGN::x}	# 'x' is an unknown assignment operator

	# When parsing an assignment operator fails because the operator is
	# incomplete, make falls back to the SysV modifier.
	@echo ${SYSV::=sysv\:x}${SYSV::x=:y}

	@echo ${ASSIGN::=value	# missing closing brace

mod-assign-shell-error:
	# If the command succeeds, the variable is assigned.
	@${SH_OK::!= echo word; true } echo ok=${SH_OK}

	# If the command fails, the variable keeps its previous value.
	# FIXME: the error message says: "previous" returned non-zero status
	@${SH_ERR::=previous}
	@${SH_ERR::!= echo word; false } echo err=${SH_ERR}

# XXX: The ::= modifier expands its right-hand side, exactly once.
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
