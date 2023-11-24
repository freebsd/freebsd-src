# $NetBSD: opt-debug-cond.mk,v 1.3 2022/04/15 09:33:20 rillig Exp $
#
# Tests for the -dc command line option, which adds debug logging for the
# evaluation of conditional expressions, such as in .if directives and
# ${cond:?then:else} expressions.

.MAKEFLAGS: -dc

# expect: CondParser_Eval: ${:U12345} > ${:U55555}
# expect: Comparing 12345.000000 > 55555.000000
.if ${:U12345} > ${:U55555}

# expect: CondParser_Eval: "string" != "string"
# expect: Comparing "string" != "string"
.elif "string" != "string"

# expect: CondParser_Eval: "nonempty"
.elif "nonempty"

.endif

.MAKEFLAGS: -d0

all: .PHONY
