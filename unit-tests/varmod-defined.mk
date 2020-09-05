# $NetBSD: varmod-defined.mk,v 1.3 2020/08/25 21:58:08 rillig Exp $
#
# Tests for the :D variable modifier, which returns the given string
# if the variable is defined.  It is closely related to the :U modifier.

DEF=	defined
.undef UNDEF

# Since DEF is defined, the value of the expression is "value", not
# "defined".
#
.if ${DEF:Dvalue} != "value"
.error
.endif

# Since UNDEF is not defined, the "value" is ignored.  Instead of leaving the
# expression undefined, it is set to "", exactly to allow the expression to
# be used in .if conditions.  In this place, other undefined expressions
# would generate an error message.
# XXX: Ideally the error message would be "undefined variable", but as of
# 2020-08-25 it is "Malformed conditional".
#
.if ${UNDEF:Dvalue} != ""
.error
.endif

all:
	@:;
