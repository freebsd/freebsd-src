# $NetBSD: hanoi-include.mk,v 1.5 2023/10/19 18:24:33 rillig Exp $
#
# Implements the Towers of Hanoi puzzle, demonstrating a bunch of more or less
# useful programming techniques:
#
#	* default assignment using the ?= assignment operator
#	* including the same file recursively (rather unusual)
#	* extracting the current value of a variable using the .for loop
#	* using the :: dependency operator for adding commands to a target
#	* on-the-fly variable assignment expressions using the ::= modifier
#
# usage:
#	env N=3 make -r -f hanoi-include.mk
#
# Specifying N in the command line instead of in the environment would produce
# an endless loop, since variables from the command line cannot be overridden
# by global variables:
#	make -r -f hanoi-include.mk N=3

N?=	5			# Move this number of disks ...
FROM?=	A			# ... from this stack ...
VIA?=	B			# ... via this stack ...
TO?=	C			# ... to this stack.

# Since make has no built-in arithmetic functions, convert N to a list of
# words and use the built-in word counting instead.
.if ${N:[#]} == 1
N:=	count ${:U:${:Urange=$N}}	# 'count' + one word for every disk
.endif

.if ${N:[#]} == 2
.  for from to in ${FROM} ${TO}
all::
	@echo "Move the upper disk from stack ${from} to stack ${to}."
.  endfor
.else
_:=	${N::=${N:[1..-2]}} ${TMP::=${VIA}} ${VIA::=${TO}} ${TO::=${TMP}}
.  include "${.PARSEDIR}/${.PARSEFILE}"
_:=	${N::+=D} ${TMP::=${VIA}} ${VIA::=${TO}} ${TO::=${TMP}}

.  for from to in ${FROM} ${TO}
all::
	@echo "Move the upper disk from stack ${from} to stack ${to}."
.  endfor

_:=	${N::=${N:[1..-2]}} ${TMP::=${VIA}} ${VIA::=${FROM}} ${FROM::=${TMP}}
.  include "${.PARSEDIR}/${.PARSEFILE}"
_:=	${N::+=D} ${TMP::=${VIA}} ${VIA::=${FROM}} ${FROM::=${TMP}}
.endif
