# $NetBSD: posix-varassign.mk,v 1.1 2025/04/13 09:29:33 rillig Exp $
#
# https://pubs.opengroup.org/onlinepubs/9799919799/utilities/make.html#tag_20_76_13_05
#
# Test that variable assignments work in the same way as in default mode.
#
# The assignment operators "::=" and ":::=" are intentionally not supported,
# as they would introduce the distinction between eagerly and lazily evaluated
# macros, in addition to the eagerly and lazily evaluated assignments, and
# this would add too much complexity to the user's mental model, for too
# little benefit.

.POSIX:


VAR=	value
.if ${VAR} != "value"
.  error
.endif


# Deviation from POSIX: The "::=" assignment operator is not supported,
# instead, the variable named "VAR:" is defined.
VAR=	before
VAR::=	posix-immediate-expansion
.if ${VAR} != "before"
.  error
.elif ${${:UVAR\:}} != "posix-immediate-expansion"
.  error
.endif


# Deviation from POSIX: The ":::=" assignment operator is not supported,
# instead, the variable named "VAR::" is defined.
VAR:::=	posix-delayed-expansion
.if ${VAR} != "before"
.  error
.elif ${${:UVAR\:\:}} != "posix-delayed-expansion"
.  error
.endif


VAR!=	echo from shell command
.if ${VAR} != "from shell command"
.  error
.endif


VAR=	value
VAR?=	fallback
.if ${VAR} != "value"
.  error
.endif
.undef VAR
VAR?=	fallback
.if ${VAR} != "fallback"
.  error
.endif


VAR=	value
VAR+=	appended
.if ${VAR} != "value appended"
.  error
.endif


# In POSIX mode, the ":=" assignment operator is available as well, even
# though it is not specified by POSIX, due to the differences in existing
# make implementations.
REF=	before
VAR:=	immediate ${REF}
REF=	after
.if ${VAR} != "immediate before"
.  error
.endif


all:
