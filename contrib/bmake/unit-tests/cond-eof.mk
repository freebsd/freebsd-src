# $NetBSD: cond-eof.mk,v 1.3 2021/12/10 23:12:44 rillig Exp $
#
# Tests for parsing conditions, especially the end of such conditions, which
# are represented as the token TOK_EOF.

SIDE_EFFECT=	${:!echo 'side effect' 1>&2!}
SIDE_EFFECT2=	${:!echo 'side effect 2' 1>&2!}

# In the following conditions, ${SIDE_EFFECT} is the position of the first
# parse error.  Before cond.c 1.286 from 2021-12-10, it was always fully
# evaluated, even if it was not necessary to expand the variable expression.
# These syntax errors are an edge case that does not occur during normal
# operation.  Still, it is easy to avoid evaluating these expressions, just in
# case they have side effects.
.if 0 ${SIDE_EFFECT} ${SIDE_EFFECT2}
.endif
.if 1 ${SIDE_EFFECT} ${SIDE_EFFECT2}
.endif
.if (0) ${SIDE_EFFECT} ${SIDE_EFFECT2}
.endif
