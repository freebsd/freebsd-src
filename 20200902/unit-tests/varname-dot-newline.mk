# $NetBSD: varname-dot-newline.mk,v 1.3 2020/08/19 05:51:18 rillig Exp $
#
# Tests for the special .newline variable.
#
# Contrary to the special variable named "" that is used in expressions like
# ${:Usome-value}, the variable ".newline" is not protected against
# modification.  Nobody exploits that though.

NEWLINE:=	${.newline}

.newline=	overwritten

.if ${.newline} == ${NEWLINE}
.info The .newline variable cannot be overwritten.  Good.
.else
.info The .newline variable can be overwritten.  Just don't do that.
.endif

# Restore the original value.
.newline=	${NEWLINE}

all:
	@echo 'first${.newline}second'
