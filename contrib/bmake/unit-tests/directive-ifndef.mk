# $NetBSD: directive-ifndef.mk,v 1.6 2020/10/24 08:46:08 rillig Exp $
#
# Tests for the .ifndef directive, which can be used for multiple-inclusion
# guards.  In contrast to C, where #ifndef and #define nicely line up the
# macro name, there is no such syntax in make.  Therefore, it is more
# common to use .if !defined(GUARD) instead.

.ifndef GUARD
GUARD=	# defined
.  info guarded section
.endif

.ifndef GUARD
GUARD=	# defined
.  info guarded section
.endif

.if !defined(GUARD)
GUARD=	# defined
.  info guarded section
.endif

all:
	@:;
