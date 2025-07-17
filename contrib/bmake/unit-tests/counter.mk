# $NetBSD: counter.mk,v 1.6 2021/04/04 10:13:09 rillig Exp $
#
# Demonstrates how to let make count the number of times a variable
# is actually accessed, using the ::= variable modifier.
#
# This works since 2020-09-23.  Before that, the counter ended up at having
# 4 words, even though the NEXT variable was only accessed 3 times.
# The cause for this surprising behavior was that the ::= variable modifiers
# returned an error marker instead of a simple empty string.

RELEVANT=	yes (load-time part)	# just to filter the output

COUNTER=	# zero

NEXT=		${COUNTER::=${COUNTER} a}${COUNTER:[#]}

# This variable is first set to empty and then expanded.
# See parse.c, function Parse_Var, keyword "!Var_Exists".
A:=		${NEXT}
B:=		${NEXT}
C:=		${NEXT}

RELEVANT=	no

all:
	@: ${RELEVANT::=yes (run-time part)}
	@echo A=${A:Q} B=${B:Q} C=${C:Q} COUNTER=${COUNTER:[#]:Q}
	@: ${RELEVANT::=no}
