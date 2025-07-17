# $NetBSD: directive-elifndef.mk,v 1.3 2022/01/22 21:50:41 rillig Exp $
#
# Tests for the .elifndef directive, which is an obscure form of writing the
# more usual '.elif !defined(VAR)'.

# At this point, VAR is not yet defined, and due to the 'n' in 'elifndef' the
# condition evaluates to true.
.if 0
.elifndef VAR && VAR || VAR
.else
.  error
.endif

VAR=	# defined

# At this point, VAR is defined, and due to the 'n' in 'elifndef' the
# condition evaluates to false.
.if 0
.elifndef VAR && VAR || VAR
.  error
.endif

all:
