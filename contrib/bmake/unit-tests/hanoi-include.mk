# $NetBSD: hanoi-include.mk,v 1.3 2022/05/08 07:27:50 rillig Exp $
#
# Implements the Towers of Hanoi puzzle, demonstrating a bunch of more or less
# useful programming techniques:
#
#	* default assignment using the ?= assignment operator
#	* including the same file recursively (rather unusual)
#	* extracting the current value of a variable using the .for loop
#	* using shell commands for calculations since make is a text processor
#	* using the :: dependency operator for adding commands to a target
#	* on-the-fly variable assignment expressions using the ::= modifier
#
# usage:
#	env N=3 make -r -f hanoi-include.mk
#
# endless loop, since command line variables cannot be overridden:
#	make -r -f hanoi-include.mk N=3

N?=	5			# Move this number of disks ...
FROM?=	A			# ... from this stack ...
VIA?=	B			# ... via this stack ...
TO?=	C			# ... to this stack.

.if $N == 1
.  for from to in ${FROM} ${TO}
all::
	@echo "Move the upper disk from stack ${from} to stack ${to}."
.  endfor
.else
_:=	${N::!=expr $N - 1} ${TMP::=${VIA}} ${VIA::=${TO}} ${TO::=${TMP}}
.  include "${.PARSEDIR}/${.PARSEFILE}"
_:=	${N::!=expr $N + 1} ${TMP::=${VIA}} ${VIA::=${TO}} ${TO::=${TMP}}

.  for from to in ${FROM} ${TO}
all::
	@echo "Move the upper disk from stack ${from} to stack ${to}."
.  endfor

_:=	${N::!=expr $N - 1} ${TMP::=${VIA}} ${VIA::=${FROM}} ${FROM::=${TMP}}
.  include "${.PARSEDIR}/${.PARSEFILE}"
_:=	${N::!=expr $N + 1} ${TMP::=${VIA}} ${VIA::=${FROM}} ${FROM::=${TMP}}
.endif
