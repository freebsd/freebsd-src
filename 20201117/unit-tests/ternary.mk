# $NetBSD: ternary.mk,v 1.2 2020/10/24 08:34:59 rillig Exp $

all:
	@for x in "" A= A=42; do ${.MAKE} -f ${MAKEFILE} show $$x; done

show:
	@echo "The answer is ${A:?known:unknown}"
	@echo "The answer is ${A:?$A:unknown}"
	@echo "The answer is ${empty(A):?empty:$A}"
