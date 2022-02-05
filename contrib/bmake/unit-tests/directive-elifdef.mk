# $NetBSD: directive-elifdef.mk,v 1.3 2022/01/22 16:23:56 rillig Exp $
#
# Tests for the .elifdef directive, which is seldom used.  Instead of writing
# '.elifdef VAR', the usual form is the more versatile '.elif defined(VAR)'.

# At this point, VAR is not defined, so the condition evaluates to false.
.if 0
.elifdef VAR
.  error
.endif

VAR=	# defined

# At this point, VAR is defined, so the condition evaluates to true.
.if 0
.elifdef VAR
.else
.  error
.endif

all:
