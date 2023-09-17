# $NetBSD: directive-ifndef.mk,v 1.8 2023/06/19 20:44:06 rillig Exp $
#
# Tests for the .ifndef directive, which can be used for multiple-inclusion
# guards.  In contrast to C, where #ifndef and #define nicely line up the
# macro name, there is no such syntax in make.  Therefore, it is more
# common to use .if !defined(GUARD) instead.
#
# See also:
#	directive-include-guard.mk

.ifndef GUARD
GUARD=	# defined
# expect+1: guarded section
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


# The '.ifndef' directive can be used with multiple arguments, even negating
# them.  Since these conditions are confusing for humans, they should be
# replaced with easier-to-understand plain '.if' directives.
DEFINED=
.ifndef UNDEFINED && UNDEFINED
.else
.  error
.endif
.ifndef UNDEFINED && DEFINED
.  error
.endif
.ifndef DEFINED && DEFINED
.  error
.endif
.ifndef !UNDEFINED && !UNDEFINED
.  error
.endif
.ifndef !UNDEFINED && !DEFINED
.  error
.endif
.ifndef !DEFINED && !DEFINED
.else
.  error
.endif

all:
