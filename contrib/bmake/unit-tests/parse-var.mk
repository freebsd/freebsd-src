# $NetBSD: parse-var.mk,v 1.1 2020/10/04 06:53:15 rillig Exp $

.MAKEFLAGS: -dL

# In variable assignments, there may be spaces on the left-hand side of the
# assignment, but only if they occur inside variable expressions.
VAR.${:U param }=	value
.if ${VAR.${:U param }} != "value"
.  error
.endif

all:
	@:;
