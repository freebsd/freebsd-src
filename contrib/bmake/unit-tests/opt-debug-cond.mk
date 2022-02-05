# $NetBSD: opt-debug-cond.mk,v 1.2 2022/01/23 16:09:38 rillig Exp $
#
# Tests for the -dc command line option, which adds debug logging for the
# evaluation of conditional expressions, such as in .if directives and
# ${cond:?then:else} expressions.

.MAKEFLAGS: -dc

# expect: CondParser_Eval: ${:U12345} > ${:U55555}
# expect: lhs = 12345.000000, rhs = 55555.000000, op = >
.if ${:U12345} > ${:U55555}

# expect: CondParser_Eval: "string" != "string"
# expect: lhs = "string", rhs = "string", op = !=
.elif "string" != "string"

# expect: CondParser_Eval: "nonempty"
.elif "nonempty"

.endif

.MAKEFLAGS: -d0

all: .PHONY
